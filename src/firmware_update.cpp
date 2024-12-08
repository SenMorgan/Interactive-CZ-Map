#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include "constants.h"
#include "leds.h"
#include "firmware_update.h"

/**
 * @brief Performs the firmware update using the provided firmware URL.
 *
 * Downloads the firmware from the specified HTTPS URL and applies the update.
 * If the update is successful, the device restarts to run the new firmware.
 *
 * @param firmwareUrl The HTTPS URL from which to download the firmware binary.
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
        // Determine if the error is from the HTTP client and retrieve the error message
        String httpClientError = http.errorToString(httpCode);
        String details = httpClientError.isEmpty() ? "HTTP request returned: " + String(httpCode) : "HTTP client error: " + httpClientError;
        publishResult(false, details.c_str());
        // Clean up the HTTP connection
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
    uint32_t written = 0;
    const size_t bufferSize = 2048;
    uint8_t buffer[bufferSize] = {0};
    size_t bytesRead = 0;

    // Variable to track the last progress percentage - used to print log and update the progress indicator
    uint8_t lastProgress = 0;

    // Prepare the progress indication
    startProgressIndication();

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

        // Calculate the progress percentage
        uint8_t progress = (written * 100) / contentLength;
        // Print log and update the progress indicator every 1% change
        if (progress != lastProgress)
        {
            Serial.printf("Firmware update progress: %u%% (%u / %u bytes)\n", progress, written, contentLength);
            progressIndicator(progress, CRGB::Blue);
            lastProgress = progress;
        }
    }

    // Verify that we have written the exact amount of data
    if (written != (uint32_t)contentLength)
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
            delay(1000);   // Delay to allow the message to be published
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

    // Do not stop progress indication here, as the device will reboot on success.
    // Otherwise stuck in the progress indication to indicate the failure.
}