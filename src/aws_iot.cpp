#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "aws_iot.h"
#include "constants.h"
#include "secrets.h"
#include "leds.h"

// Initialize Wi-Fi and MQTT client
WiFiClientSecure net;
PubSubClient client(net);

void messageHandler(char *topic, byte *payload, unsigned int length)
{
    Serial.print(F("IoT message arrived. Topic: "));
    Serial.print(topic);
    Serial.print(F(". Message: "));
    for (unsigned int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
    }
    Serial.println();

    // Allocate the JSON document
    StaticJsonDocument<1024> doc;

    // Parse the JSON document
    DeserializationError error = deserializeJson(doc, payload, length);
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }

    // Extract the LEDs array
    JsonArray ledsArray = doc["leds"];
    if (!ledsArray.isNull())
    {
        for (JsonObject ledObj : ledsArray)
        {
            int index = ledObj["index"];
            int brightness = ledObj["brightness"] | -1;
            int blinks = ledObj["blinks"] | -1;
            int delayTime = ledObj["delay"] | -1;

            // Update the LED behavior
            updateLeds(index, brightness, blinks, delayTime);
        }
    }
}