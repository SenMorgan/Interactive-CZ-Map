#include <FastLED.h>
#include "leds.h"
#include "constants.h"

// Task parameters
#define LEDS_TASK_FREQUENCY_HZ (100U)
#define LEDS_TASK_STACK_SIZE   (2 * 1024U)
#define LEDS_TASK_PRIORITY     (tskIDLE_PRIORITY + 1)
#define LEDS_TASK_CORE         1 // Core 0 is used by the WiFi

// Circle effect parameters
#define CIRCLE_EFFECT_FADE_DURATION 1000
#define CIRCLE_EFFECT_BRIGHTNESS    50
const uint8_t CIRCLE_LEDS_ARRAY[] = {0, 1, 2, 3, 4, 5, 6, 7, 9, 10, 12, 16, 17, 19, 21, 24, 25, 29, 30, 31,
                                     35, 36, 37, 42, 44, 50, 54, 56, 60, 61, 64, 65, 66, 67, 68, 69, 70, 71};

// Initialize array with number of LEDs
CRGB leds[LEDS_COUNT];

// Enum for fade directions
enum FadeDirection
{
    FADE_IN,
    FADE_OUT
};

// Struct to hold the state of an individual LED
struct LedState
{
    uint8_t brightness = 0;             // Brightness level (0-255)
    uint16_t fadeDuration = 0;          // Duration of the fade effect in milliseconds
    int16_t fadeCycles = 0;             // Number of times to perform the effect (-1 for infinite)
    CRGB color = CRGB::Black;           // Color of the LED
    bool isFading = false;              // Flag to indicate if the LED is currently fading
    bool useFadeIn = true;              // Flag to indicate if the LED should fade in or just fade out
    uint32_t startTime = 0;             // Time when the effect started
    FadeDirection direction = FADE_OUT; // Current fade direction
};

// Array to hold the state of all LEDs
LedState ledStates[LEDS_COUNT];

/**
 * @brief Resets the states of all LEDs.
 *
 * This function iterates through all LEDs and sets their color to black,
 * effectively turning them off. It also resets the state of each LED to
 * its default state.
 *
 * @note This function does not update the LED strip. Call FastLED.show() after
 */
void resetLedsStates()
{
    for (uint8_t i = 0; i < LEDS_COUNT; i++)
    {
        leds[i] = CRGB::Black;
        ledStates[i] = LedState();
    }
}

/**
 * @brief Applies a circular LED effect with the specified color.
 *
 * This function resets the state of all LEDs and then sets the LEDs
 * in the circle to the desired color with specific brightness and fade duration.
 *
 * @param color The color to set the circle LEDs to.
 */
void circleLedEffect(CRGB color)
{
    // Reset all LEDs
    resetLedsStates();

    // Set circle LEDs to the desired color
    for (uint8_t i : CIRCLE_LEDS_ARRAY)
        setLed(i, CIRCLE_EFFECT_BRIGHTNESS, CIRCLE_EFFECT_FADE_DURATION, LOOP_INDEFINITELY, color);
}

/**
 * @brief Sets the state of an LED at a specified index. If any of the parameters are out of bounds,
 * then the parameters will be ignored and a message will be printed to the serial monitor.
 *
 * @param index The index of the LED to set [0, LEDS_COUNT - 1].
 * @param brightness The brightness level to set for the LED [0, 255].
 * @param fadeDuration The delay for the fade effect in milliseconds [0, MAX_FADE_DURATION].
 * @param fadeCycles The number of times the fade effect should repeat [1, MAX_FADE_REPEATS]. Use LOOP_INDEFINITELY for infinite.
 * @param color The color to set for the LED from the CRGB color palette.
 * @param useFadeIn Flag to indicate if the LED should fade in or just fade out.
 */
void setLed(uint8_t index, uint8_t brightness, uint16_t fadeDuration, int16_t fadeCycles, CRGB color, bool useFadeIn)
{
    // Check if index is within bounds
    if (index >= LEDS_COUNT)
    {
        Serial.printf("Index [%d] is out of bounds [0, %d]\n", index, LEDS_COUNT - 1);
        return;
    }

    // Get the LED state from the array
    LedState &state = ledStates[index];

    // Validate and set the brightness
    if (brightness <= 255)
        state.brightness = brightness;
    else
        Serial.printf("Brightness [%d] is out of bounds [0, 255]\n", brightness);

    // Validate and set the fade delay
    if (fadeDuration <= MAX_FADE_DURATION)
        state.fadeDuration = fadeDuration;
    else
        Serial.printf("Fade delay [%d] is out of bounds [0, %d]\n", fadeDuration, MAX_FADE_DURATION);

    // Validate and set the fade count. This parameter starts the effect if it is greater than 0 or LOOP_INDEFINITELY for infinite.
    if ((fadeCycles > 0 && fadeCycles <= MAX_FADE_REPEATS) || fadeCycles == LOOP_INDEFINITELY)
        state.fadeCycles = fadeCycles;
    else
        Serial.printf("Fade cycles [%d] is out of bounds [1, %d] or not LOOP_INDEFINITELY for infinite\n", fadeCycles, MAX_FADE_REPEATS);

    // Set the color without validating
    state.color = color;

    // Initialize fading parameters
    state.isFading = true;
    state.startTime = millis();
    state.direction = useFadeIn ? FADE_IN : FADE_OUT;
    state.useFadeIn = useFadeIn;
}

/**
 * @brief Refreshes the state of the LEDs by updating their colors and brightness based on the current time and their respective states.
 *
 * This function iterates over all LEDs, checks their active state, and updates their colors and brightness according to the fade effect.
 * If the effect is active, it calculates the elapsed time since the effect started and adjusts the brightness accordingly.
 * If the effect has completed for the current cycle, it switches the fade direction or decreases the repeat count unless it's set to infinite.
 * Finally, it updates the LED strip to reflect the changes.
 */
void refreshLeds()
{
    unsigned long currentTime = millis();

    // Iterate over all LEDs
    for (uint8_t i = 0; i < LEDS_COUNT; i++)
    {
        // Get the LED state from the array
        LedState &state = ledStates[i];

        // Check if the LED is currently fading
        if (state.isFading)
        {
            // Calculate the elapsed time since the fade effect started
            unsigned long elapsed = currentTime - state.startTime;

            // Check if the elapsed time is less than the fade duration
            if (elapsed < state.fadeDuration)
            {
                // Calculate the progress of the fade effect
                float fadeProgress = (float)elapsed / state.fadeDuration;
                uint8_t currentBrightness;

                if (state.direction == FADE_IN)
                {
                    // Calculate the current brightness based on the fade progress (fade in)
                    currentBrightness = (uint8_t)(state.brightness * fadeProgress);
                }
                else // FADE_OUT
                {
                    // Calculate the current brightness based on the fade progress (fade out)
                    currentBrightness = (uint8_t)(state.brightness * (1.0 - fadeProgress));
                }

                // Set the LED color and brightness
                leds[i] = state.color;
                leds[i].nscale8_video(currentBrightness);
            }
            else
            {
                if (state.direction == FADE_IN)
                {
                    // Complete fade-in
                    leds[i] = state.color;
                    leds[i].nscale8_video(state.brightness);
                    // Switch to fade-out
                    state.direction = FADE_OUT;
                }
                else // FADE_OUT
                {
                    // Check if the fade effect should repeat and decrease the repeat count
                    if (state.fadeCycles != LOOP_INDEFINITELY)
                    {
                        // The fade effect has completed for this cycle. Decrease the repeat count.
                        state.fadeCycles--;
                        if (state.fadeCycles <= 0)
                        {
                            leds[i] = CRGB::Black;
                            state.isFading = false;
                            continue;
                        }
                    }

                    // Switch to fade-in
                    state.direction = FADE_IN;
                }

                // Reset for the next fade cycle
                state.startTime = currentTime;
            }
        }
        else if (state.fadeCycles != 0)
        {
            // Start or restart the fade effect if fadeCycles > 0 or LOOP_INDEFINITELY for infinite
            state.startTime = currentTime;
            state.isFading = true;
            state.direction = FADE_IN; // Start with fade-in
            leds[i] = CRGB::Black;
            leds[i].nscale8_video(0);
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
    resetLedsStates();
    FastLED.show();

    // Start infinite fade-in-out effect on one LED to indicate the task is running
    circleLedEffect(CRGB::Blue);

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