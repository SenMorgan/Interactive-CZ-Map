#include <FastLED.h>
#include "leds.h"
#include "constants.h"

// Task parameters
#define LEDS_TASK_FREQUENCY_HZ (100U)
#define LEDS_TASK_STACK_SIZE   (2 * 1024U)
#define LEDS_TASK_PRIORITY     (tskIDLE_PRIORITY + 1)
#define LEDS_TASK_CORE         1 // Core 0 is used by the WiFi

// Initialize array with number of LEDs
CRGB leds[LEDS_COUNT];

// Struct to hold the state of an individual LED
struct LedState
{
    int brightness = 0;          // Brightness level (0-255)
    int fadeDuration = 0;        // Duration of the fade effect in milliseconds
    int fadeCycles = 0;          // Number of times to perform the effect
    CRGB color = CRGB::Black;    // Color of the LED
    bool isFading = false;       // Flag to indicate if the LED is currently fading
    unsigned long startTime = 0; // Time when the effect started
};

// Array to hold the state of all LEDs
LedState ledStates[LEDS_COUNT];

/**
 * @brief Sets the state of an LED at a specified index. If any of the parameters are out of bounds,
 * then the parameters will be ignored and a message will be printed to the serial monitor.
 *
 * @param index The index of the LED to set [0, LEDS_COUNT - 1].
 * @param brightness The brightness level to set for the LED [0, 255].
 * @param fadeDuration The delay for the fade effect in milliseconds [0, MAX_FADE_DURATION].
 * @param fadeCycles The number of times the fade effect should repeat [1, MAX_FADE_REPEATS].
 * @param color The color to set for the LED from the CRGB color palette.
 */
void setLed(int index, int brightness, int fadeDuration, int fadeCycles, CRGB color)
{
    // Check if index is within bounds
    if (index < 0 || index >= LEDS_COUNT)
    {
        Serial.printf("Index [%d] is out of bounds [0, %d]\n", index, LEDS_COUNT - 1);
        return;
    }

    // Get the LED state from the array
    LedState &state = ledStates[index];

    // Validate and set the brightness
    if (brightness >= 0 && brightness <= 255)
        state.brightness = brightness;
    else
        Serial.printf("Brightness [%d] is out of bounds [0, 255]\n", brightness);

    // Validate and set the fade delay
    if (fadeDuration >= 0 && fadeDuration <= MAX_FADE_DURATION)
        state.fadeDuration = fadeDuration;
    else
        Serial.printf("Fade delay [%d] is out of bounds [0, %d]\n", fadeDuration, MAX_FADE_DURATION);

    // Validate and set the fade count. This parameter starts the effect if it is greater than 0.
    if (fadeCycles > 0 && fadeCycles <= MAX_FADE_REPEATS)
        state.fadeCycles = fadeCycles;
    else
        Serial.printf("Fade cycles [%d] is out of bounds [1, %d]\n", fadeCycles, MAX_FADE_REPEATS);

    // Set the color without validating
    state.color = color;
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

        // Check if the LED is currently fading
        if (state.isFading)
        {
            // Calculate the elapsed time since the fade effect started
            unsigned long elapsed = currentTime - state.startTime;

            // Check if the elapsed time is less than the fade delay
            if (elapsed < state.fadeDuration)
            {
                // Calculate the progress of the fade effect
                float fadeProgress = (float)elapsed / state.fadeDuration;
                // Calculate the current brightness based on the fade progress
                int currentBrightness = (int)(state.brightness * (1.0 - fadeProgress));
                // Set the LED color and brightness
                leds[i] = state.color;
                leds[i].nscale8_video(currentBrightness);
            }
            else
            {
                // The fade effect has completed for this cycle. Decrease the repeat count and reset flag.
                leds[i] = CRGB::Black;
                state.fadeCycles--;
                if (state.fadeCycles < 0)
                    state.fadeCycles = 0;
                state.isFading = false;
            }
        }
        else if (state.fadeCycles > 0)
        {
            // Start/repeat the effect if the repeat count is greater than 0
            state.startTime = currentTime;
            state.isFading = true;
            leds[i] = state.color;
            leds[i].nscale8_video(state.brightness);
        }
    }

    // Update the LED strip after all calculations
    FastLED.show();
}

/**
 * @brief Task to manage LED strip updates.
 *
 * This task initializes the LED strip, sets all LEDs to off, and then enters a loop
 * where it updates the LED states at a fixed frequency. The task also blinks one LED
 * to indicate the start of the task.
 *
 * @param pvParameters Pointer to the parameters passed to the task (not used).
 */
void ledsTask(void *pvParameters)
{
    const TickType_t xFrequency = pdMS_TO_TICKS(1000 / LEDS_TASK_FREQUENCY_HZ);
    TickType_t xLastWakeTime = xTaskGetTickCount();

    // Initialize LED strip
    FastLED.addLeds<WS2812B, LEDS_PIN, GRB>(leds, LEDS_COUNT);

    // Set all LEDs to off
    for (int i = 0; i < LEDS_COUNT; i++)
    {
        leds[i] = CRGB::Black;
        ledStates[i] = LedState();
    }
    FastLED.show();

    // Blink with one LED to indicate the start
    setLed(63, 255, 200, 10, CRGB::Green);

    Serial.println("ledsTask started");

    // Main task loop
    for (;;)
    {
        // Update their states
        refreshLeds();

        // Wait for the next cycle.
        xTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief Initializes the leds task.
 *
 * This function creates a new task called "ledsTask" and assigns it to a specific core.
 *
 * @note This function should be called once during the setup phase of the program.
 */
void ledsTaskInit(void)
{
    if (pdPASS != xTaskCreatePinnedToCore(ledsTask,
                                          "ledsTask",
                                          LEDS_TASK_STACK_SIZE,
                                          NULL,
                                          LEDS_TASK_PRIORITY,
                                          NULL,
                                          LEDS_TASK_CORE))
    {
        Serial.println("Failed to create ledsTask");
    }
}
