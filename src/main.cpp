#include <WiFi.h>
#include <FastLED.h>

#include "constants.h"

// Initialize array with number of LEDs
CRGB leds[LEDS_COUNT];

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
    WiFi.mode(WIFI_AP_STA);
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
    // Set all LEDs to red
    for (int i = 0; i < LEDS_COUNT; i++)
        leds[i] = CRGB::Red;
    FastLED.show();
    delay(1000);

    // Set all LEDs to green
    for (int i = 0; i < LEDS_COUNT; i++)
        leds[i] = CRGB::Green;
    FastLED.show();
    delay(1000);

    // Set all LEDs to blue
    for (int i = 0; i < LEDS_COUNT; i++)
        leds[i] = CRGB::Blue;
    FastLED.show();
    delay(1000);
}
