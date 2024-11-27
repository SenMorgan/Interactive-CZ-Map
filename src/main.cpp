#include <WiFi.h>
#include "constants.h"
#include "aws_iot.h"
#include "leds.h"

void setup()
{
    Serial.begin(115200);
    delay(10);
    Serial.print(F("\n\nConnecting to " WIFI_SSID "..."));

    // Set device as a Wi-Fi Station
    WiFi.hostname(HOSTNAME);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.print(F("\nWiFi connected!\nIP address: "));
    Serial.println(WiFi.localIP());

    initAWS();
    initLeds();
}

void loop()
{
    refreshLeds();           // Update the state of the LEDs
    maintainAWSConnection(); // Maintain the MQTT connection
    periodicStatusPublish(); // Publish the device status periodically
    yield();                 // Allow the ESP32 to perform background tasks
}