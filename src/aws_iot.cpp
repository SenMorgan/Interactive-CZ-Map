#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "aws_iot.h"
#include "constants.h"
#include "secrets.h"
#include "leds.h"

// Initialize Wi-Fi and MQTT client
WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

void messageHandler(char *topic, byte *payload, unsigned int length)
{
    Serial.print(F("IoT message arrived. Topic: "));
    Serial.print(topic);
    Serial.print(F("'. Message: '"));
    for (int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
    }
    Serial.println(F("'"));

    // Allocate the JSON document
    JsonDocument doc;

    // Parse the JSON document and check for errors
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
    }

    // TODO: Iterate over the array JsonArray leds = doc.as<JsonArray>(); for (JsonVariant led : leds) {}

    // Extract the LED index
    int index = doc["index"];
    // Extract the LED brightness
    int brightness = doc["brightness"];
    // Extract count of LED blinks
    int blinks = doc["blinks"];
    // Extract the delay between blinks in ms
    int delayTime = doc["delay"];
    // Extract the LED color
    int r = doc["color"][0];
    int g = doc["color"][1];
    int b = doc["color"][2];

    // Update the LED
    setLed(index, brightness, blinks, delayTime, CRGB(r, g, b));
}

void initAWS()
{
    // Configure WiFiClientSecure to use the AWS IoT device credentials
    net.setCACert(AWS_CERT_CA);
    net.setCertificate(AWS_CERT_CRT);
    net.setPrivateKey(AWS_CERT_PRIVATE);

    // Connect to the MQTT broker on the AWS endpoint with default port
    client.setServer(AWS_IOT_ENDPOINT, 8883);

    // Set the message callback function
    client.setCallback(messageHandler);

    Serial.print(F("Connecting to AWS IoT..."));
    while (!client.connect(THINGNAME))
    {
        Serial.print(".");
        delay(100);
    }

    // Check for connection to the AWS IoT
    if (!client.connected())
    {
        Serial.println(F("\nError: AWS IoT connection failed!"));
        return;
    }

    // Subscribe to a topic
    client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);

    Serial.println(F("\nAWS IoT connected"));
}