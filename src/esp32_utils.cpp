#include "esp32_utils.h"

void initSerial(unsigned long baudrate)
{
    Serial.begin(baudrate);
    while (!Serial)
        delay(10);
}

bool getEsp32ChipID(char *buffer, size_t size)
{
    // Ensure the buffer is large enough to hold the Chip ID and null terminator
    if (size < CHIP_ID_LENGTH)
    {
        Serial.println("Error: Buffer size too small for Chip ID.");
        return false;
    }

    // Retrieve the full 64-bit MAC address
    byte mac[6];
    WiFi.macAddress(mac);

    // Extract the last three bytes of the MAC address
    uint32_t macTrunc = (uint32_t)mac[3] << 16 | (uint32_t)mac[4] << 8 | (uint32_t)mac[5];

    // Format the truncated MAC address as a six-character uppercase hexadecimal string,
    // using snprintf for safety to prevent buffer overflows
    int written = snprintf(buffer, size, "%06X", macTrunc);

    // Check if snprintf was successful
    if (written < 0 || static_cast<size_t>(written) >= size)
    {
        Serial.println("Error: Failed to format Chip ID.");
        return false;
    }

    return true;
}