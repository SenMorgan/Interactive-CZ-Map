#ifndef ESP32_UTILS_H
#define ESP32_UTILS_H

#include <Arduino.h>
#include <WiFi.h>

#define CHIP_ID_LENGTH 7

/**
 * @brief Initializes the serial communication with the specified baud rate.
 *
 * This function begins the serial communication with the given baud rate and
 * waits until the serial port is ready. If no baud rate is specified, it defaults
 * to 115200.
 *
 * @param baudrate The baud rate for serial communication. Default is 115200.
 */
void initSerial(unsigned long baudrate = 115200);

/**
 * @brief Retrieves the ESP32's unique Chip ID based on the last three bytes of the MAC address.
 *
 * This function configures the WiFi to ensure the correct MAC address is used,
 * extracts the last three bytes of the MAC address, formats it as a six-character
 * uppercase hexadecimal string, and stores it in the provided buffer.
 *
 * @param buffer A pointer to a character array where the Chip ID will be stored.
 *               Must be at least 7 bytes long to accommodate the Chip ID and null terminator.
 * @param size   The size of the provided buffer.
 *
 * @return
 *         - `true` if the Chip ID was successfully written to the buffer.
 *         - `false` if the buffer size is insufficient.
 *
 * @note
 *       - Ensure that the `buffer` provided has enough space (at least 7 bytes).
 *       - This function does not return a pointer to internal or static memory,
 *         thus avoiding potential issues with memory safety and concurrency.
 */
bool getEsp32ChipID(char *buffer, size_t size);

#endif // ESP32_UTILS_H