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

#ifdef USE_AWS_FOR_FIRMWARE_UPDATE
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
    // Use non-secure HTTP
    http.begin(firmwareUrl);
#endif

    // Start the HTTP request
    int httpCode = http.GET();

    // Check the HTTP response code
    if (httpCode != HTTP_CODE_OK)
    {
        String details = "HTTP error: " + http.errorToString(httpCode);
        publishResult(false, details.c_str());
        http.end();
        return;
    }

    // Get the payload size
    int contentLength = http.getSize();
    if (contentLength <= 0)
    {
        String details = "Invalid content length: " + String(contentLength);
        publishResult(false, details.c_str());
        http.end();
        return;
    }

    // Prepare for update
    if (!Update.begin(contentLength))
    {
        String details = "Update.begin() failed with error: " + String(Update.errorString());
        publishResult(false, details.c_str());
        http.end();
        return;
    }

    Serial.println(F("URL and content length validated. Starting update..."));

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
            String details = "Update.write() != bytesRead. Written: " + String(written) +
                             ", Read: " + String(bytesRead);
            publishResult(false, details.c_str());
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
        String details = "Mismatch in written bytes. Expected: " + String(contentLength) +
                         ", Written: " + String(written);
        publishResult(false, details.c_str());
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
            publishResult(true, "Rebooting...");
            ESP.restart(); // Reboot to apply the new firmware
        }
        else
        {
            publishResult(false, "Update.isFinished() returned false");
        }
    }
    else
    {
        String details = "Update.end() failed with error: " + String(Update.errorString());
        publishResult(false, details.c_str());
    }
}