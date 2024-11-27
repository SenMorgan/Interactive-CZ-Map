#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "aws_iot.h"
#include "constants.h"
#include "leds.h"

// Initialize Wi-Fi and MQTT client
WiFiClientSecure net;
PubSubClient client(net);

// Function declarations for handlers
void messageHandler(char *topic, byte *payload, unsigned int length);
void handleLedsCommand(JsonDocument &doc);
void handleUpdateCommand(JsonDocument &doc);

/**
 * @brief Initializes the AWS IoT connection.
 *
 * This function configures the WiFiClientSecure with the AWS IoT device credentials,
 * sets up the MQTT client to connect to the AWS IoT endpoint, and subscribes to the
 * necessary MQTT topics. It also sets the message callback function to handle incoming
 * messages from the subscribed topics.
 *
 * The function will continuously attempt to connect to the AWS IoT endpoint until a
 * connection is established. If the connection fails, an error message is printed to
 * the Serial monitor.
 */
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
        Serial.print(F("."));
        delay(100);
    }

    // Check for connection to the AWS IoT
    if (!client.connected())
    {
        Serial.println(F("\nError: AWS IoT connection failed!"));
        return;
    }

    // Subscribe to topics
    client.subscribe(MQTT_SUB_TOPIC_ALL_COMMANDS);

    Serial.println(F("\nAWS IoT connected"));
}

/**
 * @brief Handles incoming IoT messages.
 *
 * This function is called whenever a new message arrives on a subscribed MQTT topic.
 * It prints the topic and message to the serial output, parses the message as a JSON document,
 * and dispatches the message to the appropriate handler based on the topic.
 *
 * @param topic The topic on which the message was received.
 * @param payload The payload of the message.
 * @param length The length of the payload.
 */
void messageHandler(char *topic, byte *payload, unsigned int length)
{
    Serial.printf("IoT message arrived. Topic: %s. Message: %.*s\n", topic, length, payload);

    // Allocate the JSON document
    JsonDocument doc;

    // Parse the JSON document and check for errors
    DeserializationError error = deserializeJson(doc, payload);
    if (error)
    {
        Serial.printf("deserializeJson() failed: %s\n", error.c_str());
        return;
    }

    // Dispatch to appropriate handler based on topic
    if (strcmp(topic, MQTT_SUB_TOPIC_LEDS) == 0)
        handleLedsCommand(doc);
    else if (strcmp(topic, MQTT_SUB_TOPIC_UPDATE) == 0)
        handleUpdateCommand(doc);
    else
        Serial.printf("Unknown topic received: %s\n", topic);
}

// Handler for LED commands
void handleLedsCommand(JsonDocument &doc)
{
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

    // TODO: Iterate over the array JsonArray leds = doc.as<JsonArray>(); for (JsonVariant led : leds) {}

    // Update the LED
    setLed(index, brightness, delayTime, blinks, CRGB(r, g, b));

    Serial.printf("LED Command - Index: %d, Brightness: %d, Blinks: %d, Delay: %d, Color: (%d, %d, %d)\n",
                  index, brightness, blinks, delayTime, r, g, b);
}

// Handler for Update commands
void handleUpdateCommand(JsonDocument &doc)
{
    #define FIRMWARE_URL_KEY "firmware_url"

    // Example: Handle firmware update command
    const char *firmwareUrl = doc[FIRMWARE_URL_KEY];
    if (firmwareUrl)
    {
        Serial.printf("Received firmware update URL: %s\n", firmwareUrl);
        // TODO: Implement firmware update logic here
    }
    else
    {
        Serial.println(F("Invalid update command received. No '" FIRMWARE_URL_KEY "' key found."));
    }
}
