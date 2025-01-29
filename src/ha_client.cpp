#include <WiFi.h>
#include "esp32_utils.h"
#include <ArduinoJson.h>

#include "ha_client.h"
#include "constants.h"

// Initialize Wi-Fi and MQTT client
WiFiClient haClientNet;
PubSubClient haClient(haClientNet);

// Interval for publishing device status (in milliseconds)
#define HA_STATUS_PUBLISH_INTERVAL 10 * 1000
// Initial delay before attempting to reconnect to Home Assistant MQTT Broker (in milliseconds)
#define RECONNECT_INITIAL_DELAY    100
// Maximum delay between reconnection attempts to Home Assistant MQTT Broker (in milliseconds)
#define RECONNECT_MAX_DELAY        10000
// MQTT buffer size for handling larger messages
#define MQTT_BUFFER_SIZE           512

// Task parameters
#define HA_TASK_STACK_SIZE (4 * 1024U)
#define HA_TASK_PRIORITY   (tskIDLE_PRIORITY + 1)
#define HA_TASK_CORE       1 // Core 0 is used by the WiFi

// MQTT topics
#define MQTT_SUB_TOPIC_ENABLE MQTT_BASE_TOPIC "/cmd/enable"
#define MQTT_PUB_TOPIC_STATUS MQTT_BASE_TOPIC "/status/device"

// Last time the device status was published
uint32_t lastHAPublishTime = 0;
// Counter for the number of times the device was reconnecting to the Home Assistant MQTT Broker
uint32_t haReconnectAttempts = 0;

// Variables to store MQTT topics
static char enableSubTopic[sizeof(MQTT_SUB_TOPIC_ENABLE) + CHIP_ID_LENGTH + 1];
static char statusPubTopic[sizeof(MQTT_PUB_TOPIC_STATUS) + CHIP_ID_LENGTH + 1];

// Indicates if the map is turned on (flashings allowed)
bool mapState = true;

// Forward declarations
void publishStatusHA();

/**
 * @brief Handles incoming messages from the Home Assistant MQTT broker.
 *
 * @param topic The topic on which the message was received.
 * @param payload The payload of the message.
 * @param length The length of the payload.
 */
void haMessageHandler(char *topic, byte *payload, unsigned int length)
{
    String message;
    for (unsigned int i = 0; i < length; i++)
    {
        message += (char)payload[i];
    }

    if (strcmp(topic, enableSubTopic) == 0)
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
        else
        {
            Serial.printf("Invalid message received on topic '%s': %s\n", topic, message.c_str());
        }

        // Publish the current status after receiving the enable command
        publishStatusHA();
    }
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
    // Allocate a buffer for the JSON document and serialize it
    char buffer[MQTT_BUFFER_SIZE];
    size_t serializedSize = serializeJson(doc, buffer, MQTT_BUFFER_SIZE);

    // Check if the buffer is large enough for the JSON document
    if (serializedSize >= MQTT_BUFFER_SIZE)
    {
        Serial.printf("Failed to publish message to topic '%s': Buffer (%d bytes) too small for JSON (%d bytes)\n",
                      topic, MQTT_BUFFER_SIZE, serializedSize);
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

/**
 * @brief Publishes the current status to Home Assistant via MQTT.
 */
void publishStatusHA()
{
    // Allocate the JSON document
    JsonDocument doc;

    doc["enabled"] = mapState ? "ON" : "OFF";
    doc["haReconnectAttempts"] = haReconnectAttempts;
    doc["awsReconnectAttempts"] = awsReconnectAttempts;
    doc["uptime"] = millis() / 1000;
    doc["awsMsgsReceived"] = awsMsgsReceived;

    // Publish device status to the MQTT topic
    publishJsonHA(statusPubTopic, doc);
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

/**
 * @brief Checks if the map is enabled.
 *
 * @return true if the map is enabled (flashings allowed), false otherwise.
 */
bool isMapOn()
{
#ifdef HA_MQTT_BROKER_HOST
    return mapState;
#else
    return true;
#endif
}

/**
 * @brief Populates the provided JsonObject with device information.
 *
 * @param deviceInfo A JsonObject to be populated with device information.
 */
void getDeviceInfo(JsonObject deviceInfo)
{
    deviceInfo["identifiers"] = "Interactive-CZ-Map";
    deviceInfo["name"] = "Interactive CZ Map";
    deviceInfo["model"] = "LaskaKit Interaktivní Mapa ČR";
    deviceInfo["manufacturer"] = "SenMorgan";
}

/**
 * @brief Publishes the discovery configuration for Home Assistant.
 *
 * This function publishes the discovery configuration for both a sensor and a switch
 * to Home Assistant using MQTT. The configuration includes details such as the name,
 * state topic, unique ID, and device information.
 *
 * @param clientId The client ID used to uniquely identify the device in Home Assistant.
 */
void publishDiscoveryConfig(const char *clientId)
{
    const char *stateConfigTopic = "homeassistant/sensor/int_cz_map/%s/config";
    const char *switchConfigTopic = "homeassistant/switch/int_cz_map/%s/config";

    char topicBuffer[128]; // Buffer for storing the topic

    // Create a device information that will be included in the sensor and switch configuration
    JsonObject deviceInfo;
    deviceInfo["identifiers"] = String(clientId);
    deviceInfo["name"] = "Interactive CZ Map";
    deviceInfo["model"] = "Interactive CZ Map. More info: https://github.com/SenMorgan/Interactive-CZ-Map";
    deviceInfo["manufacturer"] = "SenMorgan";

    // Allocate the JSON document for sensor
    JsonDocument stateDoc;
    stateDoc["name"] = "Interactive CZ Map Sensor";
    stateDoc["unique_id"] = String(clientId) + "_int_cz_map_sensor";
    stateDoc["state_topic"] = statusPubTopic;
    stateDoc["value_template"] = "{{ value_json }}";
    getDeviceInfo(stateDoc["device"].to<JsonObject>()); // Add device information

    // Compose topic and publish sensor config
    snprintf(topicBuffer, sizeof(topicBuffer), stateConfigTopic, clientId);
    publishJsonHA(topicBuffer, stateDoc);

    // Allocate the JSON document for switch
    JsonDocument switchDoc;
    switchDoc["name"] = "CZ Map Switch";
    switchDoc["unique_id"] = String(clientId) + "_int_cz_map_switch";
    switchDoc["command_topic"] = enableSubTopic;
    switchDoc["state_topic"] = statusPubTopic;
    switchDoc["value_template"] = "{{ value_json.enabled }}";
    getDeviceInfo(switchDoc["device"].to<JsonObject>()); // Add device information

    // Compose topic and publish switch config
    snprintf(topicBuffer, sizeof(topicBuffer), switchConfigTopic, clientId);
    publishJsonHA(topicBuffer, switchDoc);
}

/**
 * @brief Connects to the Home Assistant MQTT Broker using the provided client ID.
 *
 * This function attempts to establish a connection to the Home Assistant MQTT Broker.
 * If the connection fails, it will retry with an exponential backoff delay.
 *
 * @param clientId The client ID to use for the MQTT connection. Must be set before calling this function.
 */
void connectToHA(const char *clientId)
{
    static uint32_t reconnectDelay = 0;
    static uint32_t lastReconnectAttempt = 0;

    // Check if the client ID is set
    if (!clientId)
    {
        Serial.println(F("Error: Client ID must be set before connecting to Home Assistant MQTT Broker"));
        return;
    }

    uint32_t timeNow = millis();

    // Attempt to connect only if the delay has passed
    if (!haClient.connected() && (timeNow - lastReconnectAttempt >= reconnectDelay))
    {
        haReconnectAttempts++;          // Increment the number of reconnection attempts
        lastReconnectAttempt = timeNow; // Update the last reconnect attempt time

        if (haClient.connect(clientId, HA_MQTT_USER, HA_MQTT_PASS))
        {
            // Connection successful
            Serial.println(F("Connected to Home Assistant MQTT Broker"));
            reconnectDelay = RECONNECT_INITIAL_DELAY; // Reset reconnect delay

            // Subscribe to topics
            haClient.subscribe(enableSubTopic);

            // Publish the device status after successful connection
            publishStatusHA();
            publishDiscoveryConfig(clientId);
        }
        else
        {
            // Connection failed - apply exponential backoff
            Serial.printf("Connection to Home Assistant MQTT Broker failed, rc=%d\n", haClient.state());
            Serial.printf("Retrying in %lu ms\n", reconnectDelay);

            if (reconnectDelay < RECONNECT_MAX_DELAY / 2)
                reconnectDelay *= 2;
            else
                reconnectDelay = RECONNECT_MAX_DELAY;
        }
    }
}

/**
 * @brief Task to handle the Home Assistant MQTT client connection and communication.
 *
 * This task sets up the Home Assistant MQTT client, attempts to connect to the MQTT broker,
 * and handles the communication loop. If the connection is lost, it attempts to reconnect.
 *
 * @param pvParameters Pointer to the client ID (const char *) used for the MQTT connection.
 *
 */
void haClientTask(void *pvParameters)
{
    const char *clientId = (const char *)pvParameters;

    // Setup the Home Assistant MQTT client
    haClient.setServer(HA_MQTT_BROKER_HOST, HA_MQTT_BROKER_PORT);
    haClient.setCallback(haMessageHandler);
    haClient.setBufferSize(MQTT_BUFFER_SIZE); // Increase buffer size to handle large auto-discovery messages

    // Compose topics
    snprintf(enableSubTopic, sizeof(enableSubTopic), "%s/%s", MQTT_SUB_TOPIC_ENABLE, clientId);
    snprintf(statusPubTopic, sizeof(statusPubTopic), "%s/%s", MQTT_PUB_TOPIC_STATUS, clientId);

    // Attempt to connect to the Home Assistant MQTT Broker
    Serial.println(F("Connecting to Home Assistant MQTT Broker..."));
    Serial.printf("Client ID: %s\n", clientId);
    connectToHA(clientId);

    for (;;)
    {
        // If the client is connected, simply return
        if (haClient.loop())
        {
            periodicStatusPublishHA();
        }
        else
        {
            // Do not attempt to reconnect if WiFi is not connected or ESP does not have assigned IP
            if (WiFi.status() != WL_CONNECTED || WiFi.localIP() == INADDR_NONE)
                return;

            // If the client is not connected, attempt to reconnect
            Serial.println(F("Home Assistant MQTT client disconnected. Attempting to reconnect..."));
            connectToHA(clientId);
        }

        // Delay for a short period before the next iteration
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief Initializes the Home Assistant client task.
 *
 * @param clientId The client ID to be used for the Home Assistant client.
 * @param idLength The length of the client ID.
 */
void haClientTaskInit(char *clientId, size_t idLength)
{
    // Check if the client ID is valid
    if (!clientId || idLength == 0)
    {
        Serial.println(F("Error: Invalid client ID, cannot create Home Assistant client task"));
        return;
    }

    // Copy the client ID to a new buffer, otherwise it will be lost when the task is created
    char *clientIdCopy = new char[idLength];
    strncpy(clientIdCopy, clientId, idLength);

    if (xTaskCreatePinnedToCore(haClientTask,
                                "haClientTask",
                                HA_TASK_STACK_SIZE,
                                (void *)clientIdCopy,
                                HA_TASK_PRIORITY,
                                NULL,
                                HA_TASK_CORE) != pdPASS)
    {
        Serial.println("Failed to create haClientTask");
        delete[] clientIdCopy;
    }
}