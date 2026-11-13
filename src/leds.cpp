#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <FastLED.h>
#include "leds.h"
#include "constants.h"
#include "ha_client.h"

// Task parameters
#define LEDS_TASK_FREQUENCY_HZ (100U)
#define LEDS_TASK_STACK_SIZE   (2 * 1024U)
#define LEDS_TASK_PRIORITY     (tskIDLE_PRIORITY + 1)
#define LEDS_TASK_CORE         1 // Core 0 is used by the WiFi

// Defines how many commands can be queued for each LED
#define LED_STATES_QUEUE_LENGTH 10

// Circle effect parameters
#define CIRCLE_EFFECT_BRIGHTNESS      50
#define PROGRESS_INDICATOR_BRIGHTNESS 50

// Number of green flashes (ring) shown when the system status returns to idle
#define STATUS_SUCCESS_FADE_CYCLES 2

// Define the array of LEDs in the circle in clockwise order
const uint8_t CIRCLE_LEDS_ARRAY[] = {1, 2, 5, 7, 12, 21, 29, 31, 17, 25, 30, 36, 35, 42, 44, 56, 61, 65, 68,
                                     71, 69, 64, 67, 70, 66, 60, 54, 50, 37, 24, 19, 16, 10, 9, 6, 3, 0, 4};

// Initialize array with number of LEDs
CRGB leds[LEDS_COUNT];

// Enum for fade directions
enum FadeDirection
{
    FADE_IN,
    FADE_OUT
};

// Structure to hold the state of an individual LED
struct LedState
{
    uint8_t brightness = 0;             // Current brightness level
    uint16_t fadeDuration = 0;          // Current fade duration
    int16_t fadeCycles = 0;             // Remaining fade cycles
    CRGB color = CRGB::Black;           // Current color
    bool isFading = false;              // Is the LED currently fading
    bool useFadeIn = true;              // Should the LED fade in
    uint32_t startTime = 0;             // Time when the effect started
    FadeDirection direction = FADE_OUT; // Current fade direction
    QueueHandle_t commandQueue = NULL;  // Queue to hold the commands for the LED
};

// Array to hold the state of all LEDs
LedState ledStates[LEDS_COUNT];

// Variable to store task handle
TaskHandle_t ledsTaskHandle = NULL;

// Variable to store initial notification control pin state
bool notifyAllowed = false;

// Mutex protecting `leds`, `ledStates` and the status variables below.
// They are used by the LEDs task, the Arduino loop task and the Home Assistant task.
static SemaphoreHandle_t ledsMutex = NULL;

// Current system status and whether its indication is actually displayed on the map
static LedsStatus currentStatus = LedsStatus::IDLE;
static bool indicationShown = false;

// While true, the LEDs task does not touch the strip (it is owned by the progress indicator)
static bool progressActive = false;

// Main brightness of the whole map. It scales all effects, 255 means no limit
static uint8_t mainBrightness = 255;

// Forward declarations
void setLed(uint8_t index, uint8_t brightness, uint16_t fadeDuration, int16_t fadeCycles, CRGB color, bool useFadeIn = true);
void circleLedEffect(CRGB color, uint16_t fadeDuration, int16_t fadeCycles);
void resetLedsStates();

/**
 * @brief Locks the LEDs mutex. Does nothing if the mutex is not created yet.
 */
static inline void ledsLock()
{
    if (ledsMutex != NULL)
        xSemaphoreTake(ledsMutex, portMAX_DELAY);
}

/**
 * @brief Unlocks the LEDs mutex. Does nothing if the mutex is not created yet.
 */
static inline void ledsUnlock()
{
    if (ledsMutex != NULL)
        xSemaphoreGive(ledsMutex);
}

/**
 * @brief Reads the status of the notification control pin and updates the internal state.
 */
void getNotifyControlPinStatus()
{
    // Configure the notification control pin as input with pull-up resistor
    pinMode(NOTIFY_CTRL_PIN, INPUT_PULLUP);
    delay(10); // Small delay to allow the pin state to stabilize

    // If jumper between NOTIFY_CTRL_PIN and GND is present, then notifications are NOT allowed
    notifyAllowed = (digitalRead(NOTIFY_CTRL_PIN) == HIGH);
}

/**
 * @brief Starts the looping circle effect that belongs to the given status.
 *
 * @param status Any status except LedsStatus::IDLE.
 *
 * @note Must be called with the LEDs mutex locked.
 */
static void showStatusIndication(LedsStatus status)
{
    switch (status)
    {
        case LedsStatus::CONFIG_PORTAL:
            circleLedEffect(CRGB::Orange, CIRCLE_EFFECT_SLOW_FADE_DURATION, LOOP_INDEFINITELY);
            break;
        case LedsStatus::WIFI_CONNECTING:
            circleLedEffect(CRGB::Blue, CIRCLE_EFFECT_SLOW_FADE_DURATION, LOOP_INDEFINITELY);
            break;
        case LedsStatus::AWS_CONNECTING:
            circleLedEffect(CRGB::Purple, CIRCLE_EFFECT_FAST_FADE_DURATION, LOOP_INDEFINITELY);
            break;
        case LedsStatus::FATAL_ERROR:
            circleLedEffect(CRGB::Red, CIRCLE_EFFECT_FAST_FADE_DURATION, LOOP_INDEFINITELY);
            break;
        default:
            return;
    }

    indicationShown = true;
}

/**
 * @brief Sets the system status and updates its indication on the map.
 *
 * The call does nothing when the status has not changed, so it is safe to call it repeatedly.
 * An indication is displayed only if notifications are allowed by the jumper and the map is enabled.
 * When the status returns to LedsStatus::IDLE, the map flashes green, but only if an indication
 * was displayed before. Otherwise the running LED effects are left untouched.
 *
 * @param status The new system status.
 */
void ledsSetStatus(LedsStatus status)
{
    ledsLock();

    if (status != currentStatus)
    {
        currentStatus = status;
        bool allowed = notifyAllowed && isMapEnabled();

        if (status != LedsStatus::IDLE)
        {
            if (allowed)
                showStatusIndication(status);
        }
        else if (indicationShown)
        {
            // Replace the looping indication with a short success effect
            if (allowed)
                circleLedEffect(CRGB::Green, CIRCLE_EFFECT_FAST_FADE_DURATION, STATUS_SUCCESS_FADE_CYCLES);
            else
                resetLedsStates();

            indicationShown = false;
        }
    }

    ledsUnlock();
}

/**
 * @brief Returns the current system status set by ledsSetStatus().
 */
LedsStatus ledsGetStatus()
{
    ledsLock();
    LedsStatus status = currentStatus;
    ledsUnlock();

    return status;
}

/**
 * @brief Applies the map enable state (see isMapEnabled()) to the LEDs.
 *
 * Call it after the map was turned on or off. Turning the map off stops all running effects
 * and drops the queued commands. Turning it on restores the indication of the current status.
 */
void ledsApplyMapState()
{
    ledsLock();

    if (!isMapEnabled())
    {
        resetLedsStates();
        indicationShown = false;
    }
    else if (notifyAllowed && currentStatus != LedsStatus::IDLE && !indicationShown)
    {
        showStatusIndication(currentStatus);
    }

    ledsUnlock();
}

/**
 * @brief Sets the main brightness of the whole map.
 *
 * The main brightness scales all LED effects and status indications on top of their own brightness.
 * The value is not stored in flash and is 255 (no limit) after boot.
 * The firmware update progress indication uses its own fixed brightness.
 *
 * @param brightness The main brightness [0, 255]. 255 means no limit, 0 turns the LEDs off.
 */
void ledsSetMainBrightness(uint8_t brightness)
{
    ledsLock();

    mainBrightness = brightness;

    // The progress indication owns the strip brightness while it is active
    if (!progressActive)
        FastLED.setBrightness(mainBrightness);

    ledsUnlock();
}

/**
 * @brief Returns the main brightness set by ledsSetMainBrightness().
 */
uint8_t ledsGetMainBrightness()
{
    ledsLock();
    uint8_t brightness = mainBrightness;
    ledsUnlock();

    return brightness;
}

/**
 * @brief Resets the states of all LEDs.
 *
 * This function iterates through all LEDs and sets their color to black,
 * effectively turning them off. It also resets the state of each LED to
 * its default state.
 *
 * @note This function does not update the LED strip. Call FastLED.show() after
 * @note Must be called with the LEDs mutex locked.
 */
void resetLedsStates()
{
    FastLED.clear();

    for (uint8_t i = 0; i < LEDS_COUNT; i++)
    {
        leds[i] = CRGB::Black;
        ledStates[i].brightness = 0;
        ledStates[i].fadeDuration = 0;
        ledStates[i].fadeCycles = 0;
        ledStates[i].color = CRGB::Black;
        ledStates[i].isFading = false;
        ledStates[i].useFadeIn = true;
        ledStates[i].startTime = 0;
        ledStates[i].direction = FADE_OUT;

        // Empty the commands queue
        if (ledStates[i].commandQueue != NULL)
            xQueueReset(ledStates[i].commandQueue);
    }
}

/**
 * @brief Starts the progress indication by taking the LED strip from the LEDs task and resetting all LEDs.
 *
 * This function should be called before starting the progress indication to prevent concurrent access
 * to the LED strip by the LEDs task. Without this, the progress indication may have flickering issues.
 */
void startProgressIndication()
{
    ledsLock();

    // Stop the LEDs task from refreshing the strip
    progressActive = true;

    // Set brightness
    FastLED.setBrightness(PROGRESS_INDICATOR_BRIGHTNESS);

    // Reset all LEDs
    resetLedsStates();
    indicationShown = false;
    FastLED.show();

    ledsUnlock();
}

/**
 * @brief Stops the progress indication by resetting all LEDs and giving the LED strip back to the LEDs task.
 */
void stopProgressIndication()
{
    ledsLock();

    // Restore the main brightness
    FastLED.setBrightness(mainBrightness);

    // Reset all LEDs
    resetLedsStates();
    indicationShown = false;

    // Allow the LEDs task to refresh the strip again
    progressActive = false;

    ledsUnlock();
}

/**
 * @brief Indicates progress by lighting up LEDs from CIRCLE_LEDS_ARRAY.
 *
 * This function lights up LEDs one by one based on the provided progress percentage.
 *
 * @param progress Progress percentage (0 to 100).
 * @param color Color of the LEDs.
 *
 * @note Call startProgressIndication() first.
 */
void progressIndicator(uint8_t progress, CRGB color)
{
    // Ensure progress is within 0-100
    if (progress > 100)
        progress = 100;

    // Calculate the number of LEDs to light up
    uint8_t totalLeds = sizeof(CIRCLE_LEDS_ARRAY) / sizeof(CIRCLE_LEDS_ARRAY[0]);
    uint8_t ledsToLight = (progress * totalLeds) / 100;

    ledsLock();

    // Light up LEDs based on progress
    for (uint8_t i = 0; i < ledsToLight; i++)
        leds[CIRCLE_LEDS_ARRAY[i]] = color;

    // Update the LED strip
    FastLED.show();

    ledsUnlock();
}

/**
 * @brief Applies a circular LED effect with the specified color.
 *
 * This function resets the state of all LEDs and then sets the LEDs
 * in the circle to the desired color with specific brightness and fade duration.
 *
 * @param color The color to set the circle LEDs to.
 * @param fadeDuration The duration of the fade effect in milliseconds.
 * @param fadeCycles The number of times the effect should repeat. Use LOOP_INDEFINITELY for infinite.
 *
 * @note Must be called with the LEDs mutex locked.
 */
void circleLedEffect(CRGB color, uint16_t fadeDuration, int16_t fadeCycles)
{
    // Reset all LEDs
    resetLedsStates();

    // Set circle LEDs to the desired color
    for (uint8_t i : CIRCLE_LEDS_ARRAY)
        setLed(i, CIRCLE_EFFECT_BRIGHTNESS, fadeDuration, fadeCycles, color);
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
 *
 * @note Must be called with the LEDs mutex locked.
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

    // Set the brightness (the whole uint8_t range is valid)
    state.brightness = brightness;

    // Validate and set the fade duration
    if (fadeDuration <= MAX_FADE_DURATION)
        state.fadeDuration = fadeDuration;
    else
        Serial.printf("Fade duration [%d] is out of bounds [0, %d]\n", fadeDuration, MAX_FADE_DURATION);

    // Validate and set the fade cycles
    if ((fadeCycles > 0 && fadeCycles <= MAX_FADE_REPEATS) || fadeCycles == LOOP_INDEFINITELY)
        state.fadeCycles = fadeCycles;
    else
        Serial.printf("Fade cycles [%d] are invalid. Must be between 1 and %d or LOOP_INDEFINITELY.\n", fadeCycles, MAX_FADE_REPEATS);

    // Set the color
    state.color = color;

    // Initialize fading parameters
    state.isFading = true;
    state.startTime = millis();
    state.direction = useFadeIn ? FADE_IN : FADE_OUT;
    state.useFadeIn = useFadeIn;
}

/**
 * @brief Pushes a command to the specified LED's command queue.
 *
 * This function checks if the provided LED index is within the valid range.
 * If the index is out of bounds, it logs an error message and returns.
 * Otherwise, it retrieves the LED state from the array and attempts to send
 * the command to the LED's command queue. If the queue is full, it logs an
 * error message.
 *
 * @param index The index of the LED to which the command should be sent.
 * @param command The command to be sent to the LED.
 */
void pushLedCommand(uint8_t index, LedCommand command)
{
    // Check if index is within bounds
    if (index >= LEDS_COUNT)
    {
        Serial.printf("Index [%d] is out of bounds [0, %d]\n", index, LEDS_COUNT - 1);
        return;
    }

    // Get the LED state from the array
    LedState &state = ledStates[index];

    // The queues are created in ledsTaskInit()
    if (state.commandQueue == NULL)
        return;

    // Send the command to the queue
    if (xQueueSend(state.commandQueue, &command, 0) != pdTRUE)
    {
        Serial.printf("LED %d command queue is full\n", index);
    }
}

/**
 * @brief Refreshes the state of the LEDs by updating their colors and brightness based on the current time and their respective states.
 *
 * This function iterates over all LEDs, checks their active state, and updates their colors and brightness according to the fade effect.
 * If the effect is active, it calculates the elapsed time since the effect started and adjusts the brightness accordingly.
 * If the effect has completed for the current cycle, it switches the fade direction or decreases the repeat count unless it's set to infinite.
 * Finally, it updates the LED strip to reflect the changes.
 *
 * @note Must be called with the LEDs mutex locked.
 */
void refreshLeds()
{
    uint32_t currentTime = millis();

    // Iterate over all LEDs
    for (uint8_t i = 0; i < LEDS_COUNT; i++)
    {
        // Get the LED state from the array
        LedState &state = ledStates[i];

        // Check if the LED is currently fading
        if (state.isFading)
        {
            // Calculate the elapsed time since the fade effect started
            uint32_t elapsed = currentTime - state.startTime;

            // Check if the elapsed time is less than the fade duration
            if (state.fadeDuration > 0 && elapsed < state.fadeDuration)
            {
                // Calculate fade progress
                float fadeProgress = (float)(elapsed) / state.fadeDuration;
                uint8_t currentBrightness = 0;

                if (state.direction == FADE_IN)
                {
                    // Calculate the current brightness based on the fade progress (fade in)
                    currentBrightness = (uint8_t)(state.brightness * fadeProgress);
                }
                else // FADE_OUT
                {
                    // Calculate the current brightness based on the fade progress (fade out)
                    currentBrightness = (uint8_t)(state.brightness * (1.0f - fadeProgress));
                }

                // Update LED brightness
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
                    // Switch to fade-out for next cycle
                    state.direction = FADE_OUT;
                }
                else // FADE_OUT
                {
                    // Check if the fade effect should repeat
                    if (state.fadeCycles != LOOP_INDEFINITELY)
                    {
                        // Decrement fade cycles
                        state.fadeCycles--;
                        if (state.fadeCycles <= 0)
                        {
                            leds[i] = CRGB::Black;
                            state.fadeCycles = 0;
                            state.isFading = false;
                            continue;
                        }
                    }

                    if (state.useFadeIn)
                    {
                        // Switch to fade-in
                        state.direction = FADE_IN;
                        leds[i] = CRGB::Black;
                    }
                    else
                    {
                        // Reset brightness without fading in
                        leds[i] = state.color;
                        leds[i].nscale8_video(state.brightness);
                    }
                }

                // Reset start time for the next cycle
                state.startTime = currentTime;
            }
        }
        else
        {
            // If all fade cycles are completed, try to get a new command from the queue
            LedCommand command;
            if (state.commandQueue != NULL && xQueueReceive(state.commandQueue, &command, 0) == pdTRUE)
            {
                // Set new command without fading in
                setLed(i, command.brightness, command.fadeDuration, command.fadeCycles, command.color, false);
            }
        }
    }

    // Update the LED strip after all calculations
    FastLED.show();
}

/**
 * @brief Task to manage LED strip updates.
 *
 * This task updates the LED states at a fixed frequency. It skips the update
 * while the progress indication owns the LED strip.
 *
 * @param pvParameters Pointer to the parameters passed to the task (not used).
 */
void ledsTask(void *pvParameters)
{
    const TickType_t xFrequency = pdMS_TO_TICKS(1000 / LEDS_TASK_FREQUENCY_HZ);
    TickType_t xLastWakeTime = xTaskGetTickCount();

    Serial.println("ledsTask started");

    // Main task loop
    for (;;)
    {
        // Update LED states
        ledsLock();
        if (!progressActive)
            refreshLeds();
        ledsUnlock();

        // Wait for the next cycle.
        xTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

/**
 * @brief Initializes the LED strip and starts the leds task.
 *
 * This function initializes the LED strip, the mutex and the command queues, turns all LEDs off,
 * and then creates a new task called "ledsTask" and assigns it to a specific core.
 * Everything is ready when the function returns, so other modules can use the LEDs right away.
 *
 * @note This function should be called once during the setup phase of the program.
 */
void ledsTaskInit(void)
{
    // Get the initial status of the notification control pin
    getNotifyControlPinStatus();

    // Create the mutex protecting the LEDs data
    ledsMutex = xSemaphoreCreateMutex();
    if (ledsMutex == NULL)
        Serial.println("Failed to create ledsMutex");

    // Initialize LED strip
    FastLED.addLeds<WS2812B, LEDS_PIN, GRB>(leds, LEDS_COUNT);

    // Temporal dithering is used only below full brightness and may flicker on dim LEDs
    FastLED.setDither(DISABLE_DITHER);
    FastLED.setBrightness(mainBrightness);

    // Initialize LEDs command queues
    for (uint8_t i = 0; i < LEDS_COUNT; i++)
    {
        ledStates[i].commandQueue = xQueueCreate(LED_STATES_QUEUE_LENGTH, sizeof(LedCommand));
        if (ledStates[i].commandQueue == NULL)
        {
            Serial.printf("Failed to create commandQueue for LED %d\n", i);
        }
    }

    // Set all LEDs to off
    resetLedsStates();
    FastLED.show();

    // Create the LEDs task and assign it to a specific core
    if (xTaskCreatePinnedToCore(ledsTask,
                                "ledsTask",
                                LEDS_TASK_STACK_SIZE,
                                NULL,
                                LEDS_TASK_PRIORITY,
                                &ledsTaskHandle,
                                LEDS_TASK_CORE) != pdPASS)
    {
        Serial.println("Failed to create ledsTask");
    }
}
