#include <FastLED.h>
#include "leds.h"
#include "constants.h"

// Initialize array with number of LEDs
CRGB leds[LEDS_COUNT];

// Struct to hold LED state
struct LedState
{
    int brightness = 0;
    int blinksRemaining = 0;
    int blinkDelay = 0;
    unsigned long lastBlinkTime = 0;
    bool isOn = false;
};

// Array to hold state for each LED
LedState ledStates[LEDS_COUNT];

void initLeds()
{
    // Initialize LED strip
    FastLED.addLeds<WS2812B, LEDS_PIN, GRB>(leds, LEDS_COUNT);

    // Set all LEDs to off
    for (int i = 0; i < LEDS_COUNT; i++)
    {
        leds[i] = CRGB::Black;
        ledStates[i] = LedState();
    }
    FastLED.show();

    // Set global brightness
    FastLED.setBrightness(255);
}

void updateLeds(int index, int brightness, int blinks, int delayTime)
{
    if (index < 0 || index >= LEDS_COUNT)
        return;

    LedState &state = ledStates[index];

    if (brightness >= 0)
        state.brightness = brightness;

    if (blinks >= 0)
        state.blinksRemaining = blinks * 2; // Multiply by 2 for on/off cycles

    if (delayTime >= 0)
        state.blinkDelay = delayTime;

    state.lastBlinkTime = millis();
}

void updateLeds()
{
    unsigned long currentTime = millis();

    for (int i = 0; i < LEDS_COUNT; i++)
    {
        LedState &state = ledStates[i];

        if (state.blinksRemaining != 0)
        {
            if (currentTime - state.lastBlinkTime >= state.blinkDelay)
            {
                state.isOn = !state.isOn;
                state.lastBlinkTime = currentTime;

                if (!state.isOn)
                    state.blinksRemaining--;

                if (state.blinksRemaining == 0)
                {
                    // Keep the LED in its current state
                }
                else
                {
                    leds[i] = state.isOn ? CRGB(state.brightness, state.brightness, state.brightness) : CRGB::Black;
                    FastLED.show();
                }
            }
        }
    }
}