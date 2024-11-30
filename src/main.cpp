#include <WiFi.h>
#include "constants.h"
#include "aws_iot.h"
#include "leds.h"
#include "wifi_manager.h"

void setup()
{
    Serial.begin(115200);
    while (!Serial)
        delay(10);

    // Initialize modules
    ledsTaskInit();
    initWiFiManager();
    initAWS();
}

void loop()
{
    handleWiFi();            // Maintain WiFi connection
    maintainAWSConnection(); // Maintain the MQTT connection
    periodicStatusPublish(); // Publish the device status periodically
    yield();                 // Allow the ESP32 to perform background tasks
}