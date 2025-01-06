#include <ArduinoJson.h>
#include "config_parser.h"

// Keys to extract from the JSON document
#define JSON_CUSTOMER_NAME "customer_name"
#define JSON_BLE_HID_ADDRESS "ble_hid_address"
#define JSON_BASE_LED_ID "base_led_id"

// Global variable to store device configuration
DevConfig devConfig;

void parseConfig(JsonDocument &doc)
{
    // Extract configuration from the JSON document
    if (doc[JSON_CUSTOMER_NAME].is<String>())
        devConfig.customerName = doc[JSON_CUSTOMER_NAME].as<String>();

    if (doc[JSON_BLE_HID_ADDRESS].is<String>())
        devConfig.bleHidAddress = doc[JSON_BLE_HID_ADDRESS].as<String>();

    if (doc[JSON_BASE_LED_ID].is<int>())
        devConfig.baseLedId = doc[JSON_BASE_LED_ID].as<int>();

    // Print the configuration for debugging
    Serial.printf("Settings received:\nCustomer Name: %s\nBLE HID Address: %s\nBase LED ID: %d\n",
                  devConfig.customerName.c_str(),
                  devConfig.bleHidAddress.c_str(),
                  devConfig.baseLedId);
}