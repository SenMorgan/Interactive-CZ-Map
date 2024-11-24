#include <WiFi.h>
#include "constants.h"
#include "secrets.h"
#include "aws_iot.h"
#include "leds.h"

void setup()
{
    Serial.begin(115200);
    delay(10);
    Serial.print(F("\n\nConnecting to "));
    Serial.println(WIFI_SSID "...");

    // Set device as a Wi-Fi Station
    WiFi.hostname(HOSTNAME);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println(F("\nWiFi connected"));
    Serial.print(F("IP address: "));
    Serial.println(WiFi.localIP());

    initAWS();
    initLeds();
}

void loop()
{
    client.loop();
    yield();
}