#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "aws_iot.h"
#include "constants.h"
#include "leds_parser.h"
#include "software_update.h"

// Interval for publishing device status (in milliseconds)
#define STATUS_PUBLISH_INTERVAL 60 * 1000
// Initial delay before attempting to reconnect to AWS IoT (in milliseconds)
#define RECONNECT_INITIAL_DELAY 100
// Maximum delay between reconnection attempts to AWS IoT (in milliseconds)
#define RECONNECT_MAX_DELAY     30000
// MQTT buffer size for handling larger messages
#define MQTT_BUFFER_SIZE        8192

// Initialize Wi-Fi and MQTT client
WiFiClientSecure net;
PubSubClient client(net);

// Global variables
uint32_t lastPublishTime = 0; // Last time the device status was published

// Function declarations
void connectToAWS();
void publishStatus();
void messageHandler(char *topic, byte *payload, unsigned int length);
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

    // Increase the MQTT buffer size
    client.setBufferSize(MQTT_BUFFER_SIZE);

    // Set the message callback function
    client.setCallback(messageHandler);

    // Attempt to connect to AWS IoT
    Serial.println(F("Connecting to AWS IoT..."));
    connectToAWS();
}

/**
 * @brief Attempts to connect to AWS IoT and handles reconnection logic.
 *
 * This function tries to establish a connection to AWS IoT using the client object.
 * If the connection is successful, it resets the reconnection delay, subscribes to
 * necessary MQTT topics, and publishes the device status. If the connection fails,
 * it prints the error code and retries after an exponential backoff delay.
 */
void connectToAWS()
{
    static uint32_t reconnectDelay = RECONNECT_INITIAL_DELAY;
    static uint32_t lastReconnectAttempt = 0;

    // Attempt to connect to AWS IoT indefinitely
    if (!client.connect(THINGNAME))
    {
        uint32_t timeNow = millis();
        // Check if the last reconnection attempt was too soon
        if (timeNow - lastReconnectAttempt >= reconnectDelay)
        {
            // Connection failed - retry after delay
            Serial.printf("Connection to AWS IoT failed, rc=%d\n", client.state());
            Serial.printf("Retrying in %lu ms\n", reconnectDelay);

            // Exponential backoff with a limit
            if (reconnectDelay < RECONNECT_MAX_DELAY / 2)
                reconnectDelay *= 2;
            else
                reconnectDelay = RECONNECT_MAX_DELAY;

            // Update the last reconnection attempt time
            lastReconnectAttempt = timeNow;
        }
    }
    else
    {
        // Connection successful
        Serial.println(F("Connected to AWS IoT"));
        reconnectDelay = RECONNECT_INITIAL_DELAY;      // Reset reconnect delay
        client.subscribe(MQTT_SUB_TOPIC_ALL_COMMANDS); // Subscribe to all commands
        publishStatus();                               // Publish the device status
    }
}

/**
 * @brief Maintains the connection to AWS IoT.
 *
 * This function ensures that the MQTT client remains connected to AWS IoT and handles
 * MQTT loop processing. It should be called in the main loop to maintain the connection.
 * If the client is already connected, it simply returns. If the client is
 * not connected, it attempts to reconnect in a loop until successful.
 */
void maintainAWSConnection()
{
    // If the client is connected, simply return
    if (client.loop())
        return;

    // If the client is not connected, attempt to reconnect
    Serial.println(F("AWS IoT client disconnected. Attempting to reconnect..."));
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
#define BUFFER_SIZE 256 // Size of the buffer for serializing JSON

    // Allocate a buffer for the JSON document and serialize it
    char buffer[BUFFER_SIZE];
    size_t serializedSize = serializeJson(doc, buffer, BUFFER_SIZE);

    // Check if the buffer is large enough for the JSON document
    if (serializedSize >= BUFFER_SIZE)
    {
        Serial.printf("Failed to publish message to topic '%s': Buffer (%d bytes) too small for JSON (%d bytes)\n",
                      topic, BUFFER_SIZE, serializedSize);
        lastPublishTime = millis(); // Update last publish time to prevent rapid publishing
        return;
    }

    // Check if MQTT client is connected
    if (!client.connected())
    {
        Serial.printf("Error publishing to topic '%s': AWS IoT client not connected\n", topic);
        lastPublishTime = millis(); // Update last publish time to prevent rapid publishing
        return;
    }

    // Publish the message to the specified topic
    if (client.publish(topic, buffer))
        Serial.printf("Published %d bytes to topic '%s'\n", serializedSize, topic);
    else
        Serial.printf("Failed to publish message to topic '%s'\n", topic);

    // Set last publish time to the current time after attempting to publish
    lastPublishTime = millis();
}

/**
 * @brief Publishes the device status, including software version and other relevant information.
 *
 * This function constructs a JSON document containing the device's current status,
 * such as software version, Wi-Fi status, IP address, and any other pertinent details.
 * It then publishes this JSON document to the predefined MQTT status topic.
 */
void publishStatus()
{
    // Allocate the JSON document
    JsonDocument doc;

    // Populate the JSON document with status information
    doc["software_version"] = SOFTWARE_VERSION;
    doc["uptime"] = millis() / 1000;
    doc["reset_reason"] = esp_reset_reason();
    doc["wifi_ssid"] = WiFi.SSID();
    doc["ip_address"] = WiFi.localIP().toString();
    doc["mac_address"] = WiFi.macAddress();
    doc["hostname"] = WiFi.getHostname();

    // Publish the status message
    publishJson(MQTT_PUB_TOPIC_STATUS, doc);
}

/**
 * @brief Publishes the status periodically.
 *
 * This function checks the elapsed time since the last status publish and
 * publishes the status if the elapsed time is greater than or equal to the
 * STATUS_PUBLISH_INTERVAL.
 *
 * @note Variable lastPublishTime is updated in the publishStatus() function.
 */
void periodicStatusPublish()
{
    if (millis() - lastPublishTime >= STATUS_PUBLISH_INTERVAL)
        publishStatus();
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

    // Publish the update start message
    publishJson(MQTT_PUB_TOPIC_UPDATE_STATUS, doc);
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

    // Publish the update result message
    publishJson(MQTT_PUB_TOPIC_UPDATE_STATUS, doc);
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

    Serial.printf("IoT message arrived. Topic: %s. Size: %u bytes. Payload: %.*s\n",
                  topic, length, length > MAX_PRINTABLE_LENGTH ? MAX_PRINTABLE_LENGTH : length, payload);

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
    if (strcmp(topic, MQTT_SUB_TOPIC_LEDS) == 0)
        setLedsFromJsonDoc(doc);
    else if (strcmp(topic, MQTT_SUB_TOPIC_UPDATE) == 0)
        handleUpdateCommand(doc);
    else
        Serial.printf("Unknown topic received: %s\n", topic);
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
