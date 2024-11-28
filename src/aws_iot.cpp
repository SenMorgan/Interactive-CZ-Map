#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "aws_iot.h"
#include "constants.h"
#include "leds.h"
#include "software_update.h"

// Interval for publishing device status (in milliseconds)
#define STATUS_PUBLISH_INTERVAL 60 * 1000
// Initial delay before attempting to reconnect to AWS IoT (in milliseconds)
#define RECONNECT_INITIAL_DELAY 100
// Maximum delay between reconnection attempts to AWS IoT (in milliseconds)
#define RECONNECT_MAX_DELAY     30000

// Initialize Wi-Fi and MQTT client
WiFiClientSecure net;
PubSubClient client(net);

// Global variables
uint32_t lastPublishTime = 0; // Last time the device status was published

// Function declarations for handlers
void connectToAWS();
void publishStatus();
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

    // Attempt to connect to AWS IoT indefinitely
    while (!client.connect(THINGNAME))
    {
        // Connection failed - retry after delay
        Serial.printf("Connection to AWS IoT failed, rc=%d\n", client.state());
        Serial.printf("Retrying in %lu ms\n", reconnectDelay);
        delay(reconnectDelay);

        // Exponential backoff with a limit
        if (reconnectDelay < RECONNECT_MAX_DELAY / 2)
            reconnectDelay *= 2;
        else
            reconnectDelay = RECONNECT_MAX_DELAY;
    }

    // Connection successful
    Serial.println(F("Connected to AWS IoT"));
    reconnectDelay = RECONNECT_INITIAL_DELAY;      // Reset reconnect delay
    client.subscribe(MQTT_SUB_TOPIC_ALL_COMMANDS); // Subscribe to all commands
    publishStatus();                               // Publish the device status
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
 * @brief Publishes the device status, including software version and other relevant information.
 *
 * This function constructs a JSON document containing the device's current status,
 * such as software version, Wi-Fi status, IP address, and any other pertinent details.
 * It then publishes this JSON document to the predefined MQTT status topic.
 */
void publishStatus()
{
    if (!client.connected())
    {
        Serial.println(F("Cannot publish status: AWS IoT client not connected"));
        return;
    }

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

    // Convert JSON document to string
    char buffer[256];
    size_t n = serializeJson(doc, buffer);

    // Publish the status message
    if (client.publish(MQTT_PUB_TOPIC_STATUS, buffer))
    {
        // Optionally print the status message to the Serial monitor
        // Serial.printf("Device status published to topic: %s\n", MQTT_PUB_TOPIC_STATUS);
        // Serial.printf("Status size: %d bytes, JSON: %s\n", n, buffer);
    }
    else
    {
        Serial.println(F("Failed to publish device status"));
    }

    // Set last publish time to current time
    lastPublishTime = millis();
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

/**
 * @brief Publishes a message indicating the start of a firmware update.
 *
 * @param firmwareUrl The URL from which the firmware update will be downloaded.
 */
void publishFirmwareUpdateStart(const char *firmwareUrl)
{
    String statusMessage = "Starting firmware update from: " + String(firmwareUrl);
    Serial.println(statusMessage);

    if (!client.connected())
    {
        Serial.println(F("Cannot publish firmware update start: AWS IoT client not connected"));
        return;
    }

    // Allocate the JSON document
    JsonDocument doc;

    // Populate the JSON document with the update status
    doc["status"] = "in_progress";
    doc["message"] = statusMessage;

    // Convert JSON document to string
    char buffer[128];
    size_t n = serializeJson(doc, buffer);

    // Publish the update status message and print error if failed
    if (!client.publish(MQTT_PUB_TOPIC_UPDATE_STATUS, buffer))
    {
        Serial.println(F("Failed to publish update start message"));
    }
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

    if (!client.connected())
    {
        Serial.println(F("Cannot publish firmware update result: AWS IoT client not connected"));
        return;
    }

    // Allocate the JSON document
    JsonDocument doc;

    // Populate the JSON document with the update result
    doc["status"] = success ? "success" : "failure";
    doc["message"] = statusMessage;

    // Convert JSON document to string
    char buffer[128];
    size_t n = serializeJson(doc, buffer);

    // Publish the update result message and print error if failed
    if (!client.publish(MQTT_PUB_TOPIC_UPDATE_STATUS, buffer))
    {
        Serial.println(F("Failed to publish update result"));
    }
}

// Handler for Update commands
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
