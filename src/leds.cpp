#include <FastLED.h>
#include "leds.h"
#include "constants.h"

// Initialize array with number of LEDs
CRGB leds[LEDS_COUNT];

// Struct to hold the state of an individual LED
struct LedState
{
    int brightness = 0;          // Brightness level (0-255)
    int fadeDelay = 0;           // Time to fade / time between individual blinks
    int fadesCount = 0;          // Number of times to perform the effect
    CRGB color = CRGB::Black;    // Color of the LED
    bool isActive = false;       // Is the effect active
    unsigned long startTime = 0; // Timestamp when the effect starts
};

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

    // Blink with all leds to indicate the start
    setLed(63, 255, 500, 10, CRGB::Green);
}

/**
 * @brief Sets the state of an LED at a specified index.
 *
 * @param index The index of the LED to set. Must be within the bounds of the LED array.
 * @param brightness The brightness level to set for the LED (0-255). If negative, the brightness is not updated.
 * @param fadeDelay The delay for the fade effect in milliseconds. If negative, the fade delay is not updated.
 * @param fadesCount The number of times the fade effect should repeat. If less than 2, the effect is not repeated.
 * @param color The color to set for the LED from the CRGB color palette.
 */
void setLed(int index, int brightness, int fadeDelay, int fadesCount, CRGB color)
{
    // Check if index is within bounds
    if (index < 0 || index >= LEDS_COUNT)
        return;

    // Get the LED state from the array
    LedState &state = ledStates[index];

    // Update the LED state
    if (brightness >= 0)
        state.brightness = brightness;
    if (fadeDelay >= 0)
        state.fadeDelay = fadeDelay;
    if (fadesCount > 1)
        state.fadesCount = fadesCount;
    else
        state.fadesCount = 1;

    state.color = color;
    state.startTime = millis();
    state.isActive = true;
}

/**
 * @brief Refreshes the state of the LEDs by updating their colors and brightness based on the current time and their respective states.
 *
 * This function iterates over all LEDs, checks their active state, and updates their colors and brightness according to the fade effect.
 * If the effect is active, it calculates the elapsed time since the effect started and adjusts the brightness accordingly.
 * If the effect has completed for the current cycle, it turns off the LED and decreases the repeat count.
 * If the LED is not active but has remaining repeats, it restarts the effect.
 * Finally, it updates the LED strip to reflect the changes.
 */
void refreshLeds()
{
    unsigned long currentTime = millis();

    // Iterate over all LEDs
    for (int i = 0; i < LEDS_COUNT; i++)
    {
        // Get the LED state from the array
        LedState &state = ledStates[i];

        // Check if the effect is active
        if (state.isActive)
        {
            // Calculate the elapsed time since the effect started
            unsigned long elapsed = currentTime - state.startTime;

            // Check if the elapsed time is less than the fade delay
            if (elapsed < state.fadeDelay)
            {
                // Calculate the progress of the fade effect
                float fadeProgress = (float)elapsed / state.fadeDelay;
                // Calculate the current brightness based on the fade progress
                int currentBrightness = (int)(state.brightness * (1.0 - fadeProgress));
                // Set the LED color and brightness
                leds[i] = state.color;
                leds[i].nscale8_video(currentBrightness);
            }
            else
            {
                // The effect has completed for this cycle. Decrease the repeat count and reset flag.
                leds[i] = CRGB::Black;
                state.fadesCount--;
                state.isActive = false;
            }
        }
        else if (state.fadesCount > 0)
        {
            // Start/repeat the effect if the repeat count is greater than 0
            state.startTime = millis();
            state.isActive = true;
            leds[i] = state.color;
            leds[i].nscale8_video(state.brightness);
        }
    }

    // Update the LED strip after all calculations
    FastLED.show();
}