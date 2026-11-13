#include <WiFi.h>
#include "esp32_utils.h"    // Required for CHIP_ID_LENGTH
#include "constants.h"
#include "aws_iot.h"
#include "leds.h"
#include "wifi_manager.h"
#include "ha_client.h"

void setup()
{
    initSerial();

    // Get the ESP32 Chip ID
    char chipID[CHIP_ID_LENGTH] = "";
    if (!getEsp32ChipID(chipID, sizeof(chipID)))
        Serial.println(F("Error: Failed to get the Chip ID"));
    Serial.print(F("Initializing Interactive CZ Map device with Chip ID: "));
    Serial.println(chipID);

    // Initialize modules. They keep their own copies of the Chip ID,
    // because this buffer does not exist after setup() returns
    ledsTaskInit();
    initWiFiManager(chipID);

    // Initialize AWS IoT with the Thing Name if defined, otherwise use the Chip ID
#ifdef THINGNAME
    initAWS(THINGNAME, sizeof(THINGNAME));
#else
    initAWS(chipID, sizeof(chipID));
#endif

    // Initialize map control via Home Assistant if defined
#ifdef USE_HOME_ASSISTANT
    haClientTaskInit(chipID, sizeof(chipID));
#endif
}

void loop()
{
    handleWiFi();               // Maintain WiFi connection
    maintainAWSConnection();    // Maintain the MQTT connection
    periodicStatusPublishAWS(); // Publish the device status periodically
    delay(2);                   // Let the other tasks run instead of spinning at full speed
}
