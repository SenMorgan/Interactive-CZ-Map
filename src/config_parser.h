#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include <ArduinoJson.h>

// Structure to hold device configuration
struct DevConfig
{
    String customerName;
    String bleHidAddress;
    int baseLedId;
};

// Initialize the device configuration
extern DevConfig devConfig;

void parseConfig(JsonDocument &doc);

#endif // CONFIG_PARSER_H