#include <ArduinoJson.h>
#include <FastLED.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "constants.h"
#include "secrets.h"

// Initialize Wi-Fi and MQTT client
WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);

// Initialize array with number of LEDs
CRGB leds[LEDS_COUNT];

void messageHandler(char *topic, byte *payload, unsigned int length)
{
    Serial.print("incoming: ");
    Serial.print(topic);
    Serial.print(" - ");
    for (int i = 0; i < length; i++)
    {
        Serial.print((char)payload[i]);
    }
    Serial.println();

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

    // Blink with all LEDs for a while
    for (int i = 0; i < LEDS_COUNT; i++)
        leds[i] = CRGB::Green;
    FastLED.show();
    delay(100);
    for (int i = 0; i < LEDS_COUNT; i++)
        leds[i] = CRGB::Black;
    FastLED.show();
}

void initAWS()
{
    // Configure WiFiClientSecure to use the AWS IoT device credentials
    net.setCACert(AWS_CERT_CA);
    net.setCertificate(AWS_CERT_CRT);
    net.setPrivateKey(AWS_CERT_PRIVATE);

    // Connect to the MQTT broker on the AWS endpoint we defined earlier
    client.setServer(AWS_IOT_ENDPOINT, 8883);

    // Create a message handler
    client.setCallback(messageHandler);

    Serial.print("Connecting to AWS IoT...");

    while (!client.connect(THINGNAME))
    {
        Serial.print(".");
        delay(100);
    }

    if (!client.connected())
    {
        Serial.println("\nError: AWS IoT Timeout!");
        return;
    }

    // Subscribe to a topic
    client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);

    Serial.println("\nSuccess: AWS IoT Connected!");
}

void setup()
{
    Serial.begin(115200);
    delay(10);
    Serial.println();
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(WIFI_SSID);

    // Set device as a Wi-Fi Station
    WiFi.hostname(HOSTNAME);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());

    initAWS();

    // Initialize LED strip
    FastLED.addLeds<WS2812B, LEDS_PIN, GRB>(leds, LEDS_COUNT);

    // Set all LEDs to black
    for (int i = 0; i < LEDS_COUNT; i++)
        leds[i] = CRGB::Black;

    // Loop through all LEDs and light shortly every LED
    for (int i = 0; i < LEDS_COUNT; i++)
    {
        leds[i] = CRGB::Green;
        FastLED.show();
        delay(50);
        leds[i] = CRGB::Black;
    }
    FastLED.show(); // Disable last LED

    // Decrease brightness
    FastLED.setBrightness(20);
}

void loop()
{
    client.loop();
    yield();
}
