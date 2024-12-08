#include <WiFi.h>
#include "constants.h"
#include "aws_iot.h"
#include "leds.h"
#include "wifi_manager.h"

#include "esp32_utils.h"

void setup()
{
    initSerial();

    char chipID[CHIP_ID_LENGTH];
    getEsp32ChipID(chipID, sizeof(chipID));
    Serial.print(F("Initializing Interactive CZ Map device with Chip ID: "));
    Serial.println(chipID);

    // Initialize modules
    ledsTaskInit();
    initWiFiManager(chipID);

    // Initialize AWS IoT with the Thing Name if defined, otherwise use the Chip ID
#ifdef THINGNAME
    initAWS(THINGNAME, sizeof(THINGNAME));
#else
    initAWS(chipID, sizeof(chipID));
#endif
}

void loop()
{
    handleWiFi();            // Maintain WiFi connection
    maintainAWSConnection(); // Maintain the MQTT connection
    periodicStatusPublish(); // Publish the device status periodically
    yield();                 // Allow the ESP32 to perform background tasks
}