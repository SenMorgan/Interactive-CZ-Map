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

    // Extract the Unit ID
    const char *unit_id = doc["unit_id"];
    Serial.println("Unit ID: " + String(unit_id));

    // TODO: Implement the logic to control the LEDs
    blinkAllLeds();
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