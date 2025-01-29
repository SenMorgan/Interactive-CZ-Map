#include "ha_client.h"
#include "constants.h"
#include <ArduinoJson.h>

WiFiClient haClientNet;
PubSubClient haClient(haClientNet);

#define HA_STATUS_PUBLISH_INTERVAL 10 * 1000

const char *MAP_CONTROL_TOPIC = MQTT_BASE_TOPIC "/control";
const char *MAP_STATE_TOPIC = MQTT_BASE_TOPIC "/status/device";

// Last time the device status was published
uint32_t lastHAPublishTime = 0;
// Counter for the number of times the device was reconnecting to the Home Assistant MQTT Broker
uint32_t haReconnectAttempts = 0;

// Indicates if the map is turned on (flashings allowed)
bool mapState = true;

void haMessageHandler(char *topic, byte *payload, unsigned int length)
{
    String message;
    for (unsigned int i = 0; i < length; i++)
    {
        message += (char)payload[i];
    }

    if (String(topic) == MAP_CONTROL_TOPIC)
    {
        if (message == "ON")
        {
            mapState = true;
            Serial.println("Map turned ON");
        }
        else if (message == "OFF")
        {
            mapState = false;
            Serial.println("Map turned OFF");
        }
    }
}

void initHAClient(const char *chipID, size_t chipIDLength)
{
#ifdef HA_MQTT_BROKER_HOST
    Serial.println(F("Initializing Home Assistant MQTT client..."));

    // Compose hostname from chipID and set it
    const char *id = (String(HOSTNAME_PREFIX) + "_" + String(chipID)).c_str();

    haClient.setServer(HA_MQTT_BROKER_HOST, HA_MQTT_BROKER_PORT);
    haClient.setCallback(haMessageHandler);

    // Attempt to connect
    if (haClient.connect(id, HA_MQTT_USER, HA_MQTT_PASS))
    {
        haClient.subscribe(MAP_CONTROL_TOPIC);
        Serial.println(F("Connected to Home Assistant MQTT Broker"));
    }
    else
    {
        Serial.printf("Failed to connect to Home Assistant MQTT Broker. State: %s\n", haClient.state());
    }
#endif
}

void maintainHAConnection()
{
    if (!haClient.connected())
    {
        haReconnectAttempts++;
        while (!haClient.connected())
        {
            Serial.println("Connecting to Home Assistant MQTT Broker...");
            if (haClient.connect("ESP32_Map_Client"))
            {
                haClient.subscribe(MAP_CONTROL_TOPIC);
                Serial.println("Connected to Home Assistant MQTT Broker");
            }
            else
            {
                Serial.print("Failed, rc=");
                Serial.print(haClient.state());
                Serial.println(" try again in 5 seconds");
                delay(5000);
            }
        }
    }
    haClient.loop();
}

/**
 * @brief Publishes a JSON document to a specified MQTT topic.
 *
 * This function serializes a given JSON document and publishes it to the specified MQTT topic.
 *
 * @param topic The MQTT topic to publish the JSON document to.
 * @param doc The JSON document to be published.
 */
void publishJsonHA(const char *topic, const JsonDocument &doc)
{
#define BUFFER_SIZE 256 // Size of the buffer for serializing JSON

    // Allocate a buffer for the JSON document and serialize it
    char buffer[BUFFER_SIZE];
    size_t serializedSize = serializeJson(doc, buffer, BUFFER_SIZE);

    // Check if the buffer is large enough for the JSON document
    if (serializedSize >= BUFFER_SIZE)
    {
        Serial.printf("Failed to publish message to topic '%s': Buffer (%d bytes) too small for JSON (%d bytes)\n",
                      topic, BUFFER_SIZE, serializedSize);
        lastHAPublishTime = millis(); // Update last publish time to prevent rapid publishing
        return;
    }

    // Check if MQTT client is connected
    if (!haClient.connected())
    {
        Serial.printf("Error publishing to topic '%s': Home Assistant MQTT client not connected\n", topic);
        lastHAPublishTime = millis(); // Update last publish time to prevent rapid publishing
        return;
    }

    // Publish the message to the specified topic
    if (haClient.publish(topic, buffer))
        Serial.printf("Published %d bytes to topic '%s'\n", serializedSize, topic);
    else
        Serial.printf("Failed to publish message to topic '%s'\n", topic);

    // Set last publish time to the current time after attempting to publish
    lastHAPublishTime = millis();
}

void publishStatusHA()
{
    // Allocate the JSON document
    JsonDocument doc;

    doc["mapState"] = mapState ? "ON" : "OFF";
    doc["haReconnectAttempts"] = haReconnectAttempts;
    doc["awsReconnectAttempts"] = awsReconnectAttempts;
    doc["uptime"] = millis() / 1000;
    doc["awsMsgsReceived"] = awsMsgsReceived;

    // Publish device status to the MQTT topic
    publishJsonHA(MAP_STATE_TOPIC, doc);
}

/**
 * @brief Publishes the status periodically.
 *
 * This function checks the elapsed time since the last status publish and
 * publishes the status if the elapsed time is greater than or equal to the
 * HA_STATUS_PUBLISH_INTERVAL.
 *
 * @note Variable lastHAPublishTime is updated in the publishStatusHA() function.
 */
void periodicStatusPublishHA()
{
#ifdef HA_MQTT_BROKER_HOST
    if (millis() - lastHAPublishTime >= HA_STATUS_PUBLISH_INTERVAL)
        publishStatusHA();
#endif
}

bool isMapOn()
{
#ifdef HA_MQTT_BROKER_HOST
    return mapState;
#else
    return true;
#endif
}