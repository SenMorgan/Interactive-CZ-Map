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
    CRGB color = CRGB::Black;
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

/**
 * @brief Sets the state of an LED at a specified index.
 *
 * @param index The index of the LED to set. Must be within the range [0, LEDS_COUNT-1].
 * @param brightness The brightness level to set for the LED (0-255). If negative, the brightness is not changed.
 * @param blinks The number of times the LED should blink. If negative, the blink count is not changed.
 * @param delayTime The delay time in milliseconds between blinks. If negative, the delay time is not changed.
 * @param color The color to set for the LED.
 *
 * @note The blink count is multiplied by 2 to account for on/off cycles.
 * @note The function updates the last blink time to the current time.
 */
void setLed(int index, int brightness, int blinks, int delayTime, CRGB color)
{
    // Check if index is within bounds
    if (index < 0 || index >= LEDS_COUNT)
        return;

    // Get the LED state from the array
    LedState &state = ledStates[index];

    // Update the LED state
    state.color = color;
    state.lastBlinkTime = millis();
    if (brightness >= 0)
        state.brightness = brightness;
    if (blinks >= 0)
        state.blinksRemaining = blinks;
    if (delayTime >= 0)
        state.blinkDelay = delayTime;
}

/**
 * @brief Refreshes the state of the LEDs based on their blinking patterns.
 *
 * This function iterates through all the LEDs and updates their state
 * (on/off) based on the time elapsed since the last blink. If the LED
 * is supposed to blink, it toggles its state and updates the last blink
 * time. Once the LED has completed its designated number of blinks, it
 * is turned off.
 */
void refreshLeds()
{
    unsigned long currentTime = millis();

    // Iterate through all LEDs
    for (int i = 0; i < LEDS_COUNT; i++)
    {
        // Get the LED state from the array
        LedState &state = ledStates[i];

        // Check if the LED is blinking and it's time to toggle its state
        if (state.blinksRemaining > 0 && currentTime - state.lastBlinkTime >= state.blinkDelay)
        {
            // Toggle LED state and update blink time
            state.isOn = !state.isOn;
            state.lastBlinkTime = currentTime;

            // Decrement the blink count
            if (!state.isOn)
                state.blinksRemaining--;

            // Turn off LED if the blink count is exhausted
            if (state.blinksRemaining == 0 && state.isOn)
            {
                leds[i] = CRGB::Black;
                FastLED.show();
            }
            else
            {
                // Set LED color based on state
                leds[i] = state.isOn ? state.color : CRGB::Black;
                FastLED.show();
            }
        }
    }
}