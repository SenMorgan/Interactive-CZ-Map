#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <memory>
#include "aws_iot.h"
#include "constants.h"
#include "leds_parser.h"
#include "leds.h"
#include "firmware_update.h"
#include "ha_client.h"

// Interval for publishing device status (in milliseconds)
#define STATUS_PUBLISH_INTERVAL (60 * 1000)
// Initial delay before attempting to reconnect to AWS IoT (in milliseconds)
#define RECONNECT_INITIAL_DELAY 1000
// Maximum delay between reconnection attempts to AWS IoT (in milliseconds)
#define RECONNECT_MAX_DELAY     30000
// A connection that lasted at least this long is considered stable (in milliseconds).
// Losing a stable connection resets the reconnect delay, losing an unstable one keeps the backoff.
#define STABLE_CONNECTION_TIME 60000
// How long the connection must be down before it is indicated on the map (in milliseconds).
// Shorter outages are handled silently so that they do not interrupt the running LED effects.
#define DISCONNECT_INDICATION_DELAY 10000
// MQTT buffer size for handling larger messages
#define MQTT_BUFFER_SIZE        8192
// Maximum length of the client ID (could be extended if needed)
#define MAX_CLIENT_ID_LENGTH    32

// Initialize Wi-Fi and MQTT client
WiFiClientSecure net;
PubSubClient client(net);

// Last time the device status was published
uint32_t lastAwsPublishTime = 0;
// Counter for the number of times the device was reconnecting to AWS IoT
uint32_t awsReconnectAttempts = 0;
// Counter for the number of received messages from AWS IoT
uint32_t awsMsgsReceived = 0;

// Client ID. It is a copy, because the buffer of the caller may not outlive initAWS()
static char clientId[MAX_CLIENT_ID_LENGTH + 1] = "";

// Connection state used for the reconnect backoff and for the indication on the map
static bool isConnected = false;          // Connection state seen in the previous call
static bool everConnected = false;        // True after the first successful connection
static uint32_t connectedSince = 0;       // Time of the last successful connection
static uint32_t disconnectedSince = 0;    // Time when the connection loss was detected
static uint32_t reconnectDelay = 0;       // Delay before the next connection attempt (0 = immediately)
static uint32_t lastReconnectAttempt = 0; // Time when the last connection attempt finished

// Variables to store device-specific MQTT topics to subscribe
static char ledsSubTopic[sizeof(MQTT_SUB_TOPIC_LEDS) + MAX_CLIENT_ID_LENGTH + 1];
static char updateSubTopic[sizeof(MQTT_SUB_TOPIC_UPDATE) + MAX_CLIENT_ID_LENGTH + 1];

// Variables to store device-specific MQTT topics to publish
static char statusPubTopic[sizeof(MQTT_PUB_TOPIC_STATUS) + MAX_CLIENT_ID_LENGTH + 1];
static char updateStatusPubTopic[sizeof(MQTT_PUB_TOPIC_UPDATE_STATUS) + MAX_CLIENT_ID_LENGTH + 1];

// Function declarations
void connectToAWS();
void publishStatusAWS();
void messageHandler(char *topic, byte *payload, unsigned int length);
void handleUpdateCommand(JsonDocument &doc);

/**
 * @brief Initializes the AWS IoT connection.
 *
 * This function configures the WiFiClientSecure with the AWS IoT device credentials,
 * sets up the MQTT client to connect to the AWS IoT endpoint, and composes the
 * device-specific MQTT topics. It also sets the message callback function to handle
 * incoming messages from the subscribed topics.
 *
 * The function makes the first connection attempt. If it fails, maintainAWSConnection()
 * keeps retrying from the main loop.
 *
 * @param id Pointer to the client ID to use for the AWS IoT connection. The ID is copied.
 * @param idLength Size of the client ID buffer.
 */
void initAWS(const char *id, size_t idLength)
{
    Serial.println(F("Initializing AWS IoT client..."));

    // Check if the client ID is valid
    if (!id || idLength == 0 || id[0] == '\0')
    {
        Serial.println(F("Error: Invalid client ID"));
        return;
    }

    // Validate the client ID length
    if (strnlen(id, idLength) > MAX_CLIENT_ID_LENGTH)
    {
        Serial.println(F("Error: Client ID too long. Consider increasing MAX_CLIENT_ID_LENGTH"));
        return;
    }

    // Store a copy of the client ID
    snprintf(clientId, sizeof(clientId), "%.*s", (int)idLength, id);

    // Configure WiFiClientSecure to use the AWS IoT device credentials
    net.setCACert(AWS_CERT_CA);
    net.setCertificate(AWS_CERT_CRT);
    net.setPrivateKey(AWS_CERT_PRIVATE);

    // Connect to the MQTT broker on the AWS endpoint with default port
    client.setServer(AWS_IOT_ENDPOINT, 8883);

    // Increase the MQTT buffer size
    client.setBufferSize(MQTT_BUFFER_SIZE);

    // Set the message callback function
    client.setCallback(messageHandler);

    // Compose topics to subscribe with the client ID
    snprintf(ledsSubTopic, sizeof(ledsSubTopic), "%s/%s", MQTT_SUB_TOPIC_LEDS, clientId);
    snprintf(updateSubTopic, sizeof(updateSubTopic), "%s/%s", MQTT_SUB_TOPIC_UPDATE, clientId);

    // Compose topics to publish with the client ID
    snprintf(statusPubTopic, sizeof(statusPubTopic), "%s/%s", MQTT_PUB_TOPIC_STATUS, clientId);
    snprintf(updateStatusPubTopic, sizeof(updateStatusPubTopic), "%s/%s", MQTT_PUB_TOPIC_UPDATE_STATUS, clientId);

    // Attempt to connect to AWS IoT
    maintainAWSConnection();
}

/**
 * @brief Returns a human-readable description of a PubSubClient state code.
 *
 * @param state The value returned by PubSubClient::state().
 */
static const char *mqttStateToString(int state)
{
    switch (state)
    {
        case MQTT_CONNECTION_TIMEOUT:
            return "connection timeout";
        case MQTT_CONNECTION_LOST:
            return "connection lost";
        case MQTT_CONNECT_FAILED:
            return "network connection failed";
        case MQTT_DISCONNECTED:
            return "disconnected";
        case MQTT_CONNECTED:
            return "connected";
        case MQTT_CONNECT_BAD_PROTOCOL:
            return "bad protocol";
        case MQTT_CONNECT_BAD_CLIENT_ID:
            return "bad client ID";
        case MQTT_CONNECT_UNAVAILABLE:
            return "server unavailable";
        case MQTT_CONNECT_BAD_CREDENTIALS:
            return "bad credentials";
        case MQTT_CONNECT_UNAUTHORIZED:
            return "unauthorized";
        default:
            return "unknown";
    }
}

/**
 * @brief Makes one attempt to connect to AWS IoT.
 *
 * If the connection is successful, the function subscribes to the MQTT topics,
 * publishes the device status and clears the connection indication on the map.
 * After every attempt the reconnect delay is increased (exponential backoff).
 * The delay is reset in maintainAWSConnection() only when a stable connection is lost,
 * so a connection that drops right after it is established does not cause rapid reconnects.
 *
 * @note This function blocks during the TLS handshake.
 */
void connectToAWS()
{
    awsReconnectAttempts++; // Increment the number of reconnection attempts
    Serial.printf("Connecting to AWS IoT (attempt %lu)...\n", (unsigned long)awsReconnectAttempts);

    bool success = client.connect(clientId);

    if (success)
    {
        Serial.println(F("Connected to AWS IoT"));

        // Subscribe to the generic and to the device-specific MQTT topics
        const char *topics[] = {MQTT_SUB_TOPIC_LEDS, MQTT_SUB_TOPIC_UPDATE, ledsSubTopic, updateSubTopic};
        for (const char *topic : topics)
        {
            if (!client.subscribe(topic))
                Serial.printf("Failed to subscribe to topic '%s'\n", topic);
        }

        isConnected = true;
        everConnected = true;
        connectedSince = millis();

        // Publish the device status after successful connection
        publishStatusAWS();

        // Clear the connection indication (flashes green if it was shown)
        ledsSetStatus(LedsStatus::IDLE);
    }
    else
    {
        Serial.printf("Connection to AWS IoT failed, rc=%d (%s)\n", client.state(), mqttStateToString(client.state()));
    }

    // Apply exponential backoff for the next attempt
    if (reconnectDelay == 0)
        reconnectDelay = RECONNECT_INITIAL_DELAY;
    else if (reconnectDelay < RECONNECT_MAX_DELAY / 2)
        reconnectDelay *= 2;
    else
        reconnectDelay = RECONNECT_MAX_DELAY;

    if (!success)
        Serial.printf("Retrying in %lu ms\n", (unsigned long)reconnectDelay);

    lastReconnectAttempt = millis();
}

/**
 * @brief Maintains the connection to AWS IoT.
 *
 * This function handles MQTT loop processing and reconnects to AWS IoT when the connection
 * is lost. It should be called in the main loop. It never blocks longer than one connection attempt.
 *
 * The connection loss is indicated on the map only when it lasts longer than
 * DISCONNECT_INDICATION_DELAY, or when another indication is already displayed.
 */
void maintainAWSConnection()
{
    // Nothing to do until initAWS() succeeds
    if (clientId[0] == '\0')
        return;

    bool wifiReady = (WiFi.status() == WL_CONNECTED && WiFi.localIP() != INADDR_NONE);

    // Process incoming messages and keep-alive
    if (client.loop())
    {
        // Connected. Clear the indication left by a WiFi reconnect that the MQTT session survived
        if (wifiReady)
            ledsSetStatus(LedsStatus::IDLE);
        return;
    }

    uint32_t timeNow = millis();

    // Handle the transition from connected to disconnected
    if (isConnected)
    {
        isConnected = false;
        disconnectedSince = timeNow;
        Serial.printf("AWS IoT client disconnected, rc=%d (%s)\n", client.state(), mqttStateToString(client.state()));

        // Reconnect immediately only if the lost connection was stable
        if (timeNow - connectedSince >= STABLE_CONNECTION_TIME)
            reconnectDelay = 0;
    }

    // Do not attempt to reconnect if WiFi is not connected or ESP does not have assigned IP
    if (!wifiReady)
        return;

    // Indicate the connection attempt: always on startup, otherwise only for longer outages
    if (!everConnected || ledsGetStatus() != LedsStatus::IDLE || timeNow - disconnectedSince >= DISCONNECT_INDICATION_DELAY)
        ledsSetStatus(LedsStatus::AWS_CONNECTING);

    // Attempt to connect only if the delay has passed
    if (reconnectDelay == 0 || timeNow - lastReconnectAttempt >= reconnectDelay)
        connectToAWS();
}

/**
 * @brief Publishes a JSON document to a specified MQTT topic.
 *
 * This function serializes a given JSON document and publishes it to the specified
 * MQTT topic using the AWS IoT client.
 *
 * @param topic The MQTT topic to publish the JSON document to.
 * @param doc The JSON document to be published.
 */
void publishJson(const char *topic, const JsonDocument &doc)
{
    // Set last publish time even if publishing fails, to prevent rapid publishing
    lastAwsPublishTime = millis();

    // Check if MQTT client is connected
    if (!client.connected())
    {
        Serial.printf("Error publishing to topic '%s': AWS IoT client not connected\n", topic);
        return;
    }

    // Check if the message fits into the MQTT buffer (fixed header + topic + payload)
    size_t jsonSize = measureJson(doc);
    if (jsonSize + strlen(topic) + MQTT_MAX_HEADER_SIZE + 2 > MQTT_BUFFER_SIZE)
    {
        Serial.printf("Failed to publish message to topic '%s': JSON (%u bytes) too big for the MQTT buffer\n",
                      topic, (unsigned)jsonSize);
        return;
    }

    // Allocate a buffer for the JSON document and serialize it
    std::unique_ptr<char[]> buffer(new (std::nothrow) char[jsonSize + 1]);
    if (!buffer)
    {
        Serial.printf("Failed to publish message to topic '%s': out of memory\n", topic);
        return;
    }
    serializeJson(doc, buffer.get(), jsonSize + 1);

    // Publish the message to the specified topic
    if (client.publish(topic, buffer.get()))
        Serial.printf("Published %u bytes to topic '%s'\n", (unsigned)jsonSize, topic);
    else
        Serial.printf("Failed to publish message to topic '%s'\n", topic);
}

/**
 * @brief Publishes the device status, including firmware version and other relevant information.
 *
 * This function constructs a JSON document containing the device's current status,
 * such as firmware version, Wi-Fi status, IP address, and any other pertinent details.
 * It then publishes this JSON document to the predefined MQTT status topic.
 */
void publishStatusAWS()
{
    // Allocate the JSON document
    JsonDocument doc;

    // Populate the JSON document with status information
    doc["fw_version"] = FIRMWARE_VERSION;
    doc["uptime"] = millis() / 1000;
    doc["reconnects"] = awsReconnectAttempts;
    doc["reset_reason"] = esp_reset_reason();
    doc["wifi_ssid"] = WiFi.SSID();
    doc["ip_address"] = WiFi.localIP().toString();
    doc["mac_address"] = WiFi.macAddress();

    // Publish device status to the MQTT topic
    publishJson(statusPubTopic, doc);
}

/**
 * @brief Publishes the status periodically to the AWS IoT topic.
 *
 * This function checks the elapsed time since the last status publish and
 * publishes the status if the elapsed time is greater than or equal to the
 * STATUS_PUBLISH_INTERVAL.
 *
 * @note Variable lastAwsPublishTime is updated in the publishStatusAWS() function.
 */
void periodicStatusPublishAWS()
{
    if (millis() - lastAwsPublishTime >= STATUS_PUBLISH_INTERVAL)
        publishStatusAWS();
}

/**
 * @brief Publishes a message indicating the start of a firmware update.
 *
 * @param firmwareUrl The URL from which the firmware update will be downloaded.
 */
void publishFirmwareUpdateStart(const char *firmwareUrl)
{
    String statusMessage = "Starting firmware update from: " + String(firmwareUrl);
    Serial.println(statusMessage);

    // Allocate the JSON document
    JsonDocument doc;

    // Populate the JSON document with the update status
    doc["status"] = "in_progress";
    doc["message"] = statusMessage;

    // Publish FW update start message to the MQTT topic
    publishJson(updateStatusPubTopic, doc);
}

/**
 * @brief Publishes the firmware update result to the AWS IoT topic.
 *
 * @param success A boolean indicating whether the firmware update was successful.
 * @param message A C-string containing a message describing the update result.
 */
void publishFirmwareUpdateResult(bool success, const char *message)
{
    // Construct the status message
    char statusMessage[128];
    snprintf(statusMessage, sizeof(statusMessage), "Firmware update %s. %s", success ? "successful" : "failed", message);
    Serial.println(statusMessage);

    // Allocate the JSON document
    JsonDocument doc;

    // Populate the JSON document with the update result
    doc["status"] = success ? "success" : "failure";
    doc["message"] = statusMessage;

    // Publish FW update result message to the MQTT topic
    publishJson(updateStatusPubTopic, doc);
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
#define MAX_PRINTABLE_LENGTH 128 // Maximum length of payload to print

    if (length <= MAX_PRINTABLE_LENGTH)
        Serial.printf("IoT message arrived. Topic: %s. Size: %u bytes. Payload: %.*s\n",
                      topic, length, length, payload);
    else
        Serial.printf("IoT message arrived. Topic: %s. Size: %u bytes. Payload (first %d bytes): %.*s\n",
                      topic, length, MAX_PRINTABLE_LENGTH, MAX_PRINTABLE_LENGTH, payload);

    // Allocate the JSON document
    JsonDocument doc;

    // Parse the JSON document and check for errors
    DeserializationError error = deserializeJson(doc, payload, length);
    if (error)
    {
        Serial.printf("deserializeJson() failed: %s\n", error.c_str());
        return;
    }

    // Dispatch to appropriate handler based on topic
    if (strcmp(topic, ledsSubTopic) == 0 || strcmp(topic, MQTT_SUB_TOPIC_LEDS) == 0)
    {
        if (isMapEnabled()) // Parse and set LEDs only if the map is turned on
            setLedsFromJsonDoc(doc);
    }
    else if (strcmp(topic, updateSubTopic) == 0 || strcmp(topic, MQTT_SUB_TOPIC_UPDATE) == 0)
    {
        handleUpdateCommand(doc);
    }
    else
    {
        Serial.printf("Unknown topic received: %s\n", topic);
    }

    awsMsgsReceived++; // Increment the number of received messages
}

/**
 * @brief Handles the firmware update command received in a JSON document.
 *
 * This function extracts the firmware URL from the provided JSON document and initiates
 * the firmware update process if the URL is valid. It also publishes the start and result
 * of the firmware update process.
 *
 * @param doc The JSON document containing the firmware update command.
 *
 * The JSON document is expected to have the following structure:
 * {
 *     "firmware_url": "http://example.com/firmware.bin"
 * }
 */
void handleUpdateCommand(JsonDocument &doc)
{
#define FIRMWARE_URL_KEY "firmware_url"

    // Extract the firmware URL from the JSON document
    const char *firmwareUrl = doc[FIRMWARE_URL_KEY];

    if (firmwareUrl && strlen(firmwareUrl) > 0)
    {
        // Publish the start of the firmware update
        publishFirmwareUpdateStart(firmwareUrl);

        // Perform the firmware update
        performFirmwareUpdate(firmwareUrl, publishFirmwareUpdateResult);
    }
    else
    {
        publishFirmwareUpdateResult(false, "Invalid update command received: no '" FIRMWARE_URL_KEY "' key found");
    }
}
