#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include "constants.h"
#include "software_update.h"

/**
 * @brief Performs the firmware update using the provided firmware URL.
 *
 * Downloads the firmware from the specified HTTPS URL and applies the update.
 * If the update is successful, the device restarts to run the new firmware.
 *
 * @param firmwareUrl The HTTPS URL from which to download the firmware binary.
 * @param publishResult The callback function to publish the update status.
 */
void performFirmwareUpdate(const char *firmwareUrl, PublishResult publishResult)
{
    // Initialize HTTP client
    HTTPClient http;

#ifdef USE_AWS_FOR_OTA_UPDATE
    // Ensure the URL uses HTTPS for secure download
    if (strncmp(firmwareUrl, "https://", 8) == 0)
    {
        // Set the AWS root CA certificate for secure connection
        http.begin(firmwareUrl, AWS_CERT_CA);
    }
    else
    {
        publishResult(false, "Firmware URL must use HTTPS");
        return;
    }
#else
    // Use non-secure HTTP for OTA updates
    http.begin(firmwareUrl);
#endif

    // Start the HTTP request
    int httpCode = http.GET();

    if (httpCode != HTTP_CODE_OK)
    {
        String message = "Firmware update failed. HTTP error: " + http.errorToString(httpCode);
        publishResult(false, message.c_str());
        http.end();
        return;
    }

    // Get the payload size
    int contentLength = http.getSize();
    if (contentLength <= 0)
    {
        publishResult(false, "Invalid content length");
        http.end();
        return;
    }

    // Prepare for update
    if (!Update.begin(contentLength))
    {
        publishResult(false, "Firmware update failed to begin");
        http.end();
        return;
    }

    Serial.println(F("Begining OTA update..."));

    // Create a buffer to hold the firmware data
    WiFiClient *stream = http.getStreamPtr();
    size_t written = 0;
    const size_t bufferSize = 2048;
    uint8_t buffer[bufferSize] = {0};
    size_t bytesRead = 0;

    while (http.connected() && (bytesRead = stream->readBytes(buffer, bufferSize)) > 0)
    {
        if (Update.write(buffer, bytesRead) != bytesRead)
        {
            publishResult(false, "Firmware update failed to write data");
            Update.end(false); // Abort the update
            http.end();
            return;
        }
        written += bytesRead;
        Serial.printf("Written %u / %u bytes\n", written, contentLength);
    }

    // Verify that we have written the exact amount of data
    if (written != (size_t)contentLength)
    {
        String message = "Mismatch in written bytes. Expected: " + String(contentLength) +
                         ", Written: " + String(written);
        publishResult(false, message.c_str());
        Update.end(false); // Abort the update
        http.end();
        return;
    }

    // Close the stream and end the HTTP connection
    http.end();

    // Finish the update process
    if (Update.end(true)) // Pass 'true' to set the update as finished
    {
        if (Update.isFinished())
        {
            publishResult(true, "OTA update successfully completed. Rebooting...");
            ESP.restart(); // Reboot to apply the new firmware
        }
        else
        {
            publishResult(false, "OTA update failed to complete. Something went wrong!");
        }
    }
    else
    {
        String message = "Error occurred during firmware update. Error #: " + String(Update.getError());
        publishResult(false, message.c_str());
    }
}