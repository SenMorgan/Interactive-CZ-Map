#include <LittleFS.h>
#include "drd.h"

// Task parameters
#define DRD_TASK_STACK_SIZE (4 * 1024U)
#define DRD_TASK_PRIORITY   (tskIDLE_PRIORITY + 2)
#define DRD_TASK_CORE       1 // Core 0 is used by the WiFi

// Double Reset Detection parameters
#define DRD_FILENAME   "/drd_flag.dat"
#define DRD_FLAG_SET   0xD0D01234
#define DRD_FLAG_CLEAR 0xD0D04321

static uint32_t drdTimeout = 0;
static bool doubleResetDetected = false;

/**
 * @brief Checks if a double reset has been detected.
 *
 * @return true if double reset detected, false otherwise.
 */
bool isDoubleResetDetected()
{
    return doubleResetDetected;
}

/**
 * @brief Writes the flag to the filesystem.
 *
 * @param flag The flag value to write.
 */
void writeFlag(uint32_t flag)
{
    File file = LittleFS.open(DRD_FILENAME, "w");
    if (file)
    {
        file.write((uint8_t *)&flag, sizeof(flag));
        file.close();
    }
}

/**
 * @brief Check if flag is set in the filesystem.
 */
bool isFlagSetInFS()
{
    uint32_t flag = DRD_FLAG_CLEAR;
    if (LittleFS.exists(DRD_FILENAME))
    {
        File file = LittleFS.open(DRD_FILENAME, "r");
        if (file)
        {
            file.readBytes((char *)&flag, sizeof(flag));
            file.close();
        }
    }

    return flag == DRD_FLAG_SET;
}

/**
 * @brief The Double Reset Detection Task.
 *
 * @param pvParameters Pointer to DRDTaskParams containing the timeout.
 */
void drdTask(void *pvParameters)
{
    // Initialize LittleFS
    if (!LittleFS.begin())
    {
        Serial.println("Error: Failed to initialize LittleFS in drdTask");
        vTaskDelete(NULL);
        return;
    }

    // Check if flag is set
    if (isFlagSetInFS())
    {
        // Flag was set, double reset detected
        doubleResetDetected = true;
        Serial.println("Double Reset Detected");

        // Clear flag
        writeFlag(DRD_FLAG_CLEAR);
    }
    else
    {
        Serial.println("No Double Reset Detected");

        // Set flag for next reset
        writeFlag(DRD_FLAG_SET);
    }

    // Delay for the timeout duration
    vTaskDelay(pdMS_TO_TICKS(drdTimeout));

    // Clear flag after timeout
    writeFlag(DRD_FLAG_CLEAR);
    Serial.println("Double Reset Detection timeout - flag cleared");

    vTaskDelete(NULL);
}

/**
 * @brief Initializes the Double Reset Detector task.
 *
 * This function creates a new task called "drdTask" and assigns it to a specific core.
 *
 * @param timeoutMs The timeout duration in milliseconds for detecting double resets.
 *
 * @note This function should be called once during the setup phase of the program.
 */
void drdTaskInit(uint32_t timeoutMs)
{
    drdTimeout = timeoutMs;

    if (xTaskCreatePinnedToCore(drdTask,
                                "drdTask",
                                DRD_TASK_STACK_SIZE,
                                NULL,
                                DRD_TASK_PRIORITY,
                                NULL,
                                DRD_TASK_CORE) != pdPASS)
    {
        Serial.println("Failed to create drdTask");
    }
}