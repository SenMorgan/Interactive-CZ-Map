#include <FastLED.h>
#include "leds.h"
#include "constants.h"

// Initialize array with number of LEDs
CRGB leds[LEDS_COUNT];

void initLeds()
{
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

void blinkAllLeds()
{
    for (int i = 0; i < LEDS_COUNT; i++)
        leds[i] = CRGB::Green;
    FastLED.show();
    delay(100);
    for (int i = 0; i < LEDS_COUNT; i++)
        leds[i] = CRGB::Black;
    FastLED.show();
}