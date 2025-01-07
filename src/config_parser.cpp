#include <ArduinoJson.h>
#include "config_parser.h"

// Keys to extract from the JSON document
#define JSON_CUSTOMER_NAME   "customer_name"
#define JSON_BLE_HID_ADDRESS "ble_hid_address"
#define JSON_BASE_LED_ID     "base_led_id"

// Global variable to store device configuration
DevConfig devConfig;

void parseConfig(JsonDocument &doc)
{
    // Extract configuration from the JSON document
    if (doc[JSON_CUSTOMER_NAME].is<String>())
        devConfig.customerName = doc[JSON_CUSTOMER_NAME].as<String>();

    if (doc[JSON_BLE_HID_ADDRESS].is<String>())
    {
        String hidAddr = doc[JSON_BLE_HID_ADDRESS].as<String>();
        // Convert the BLE HID address to a NimBLEAddress
        devConfig.bleHidAddress = NimBLEAddress(hidAddr.c_str(), BLE_ADDR_PUBLIC);
        if (!devConfig.bleHidAddress)
            Serial.printf("Failed to convert BLE HID address: %s\n", hidAddr.c_str());
    }

    if (doc[JSON_BASE_LED_ID].is<int>())
        devConfig.baseLedId = doc[JSON_BASE_LED_ID].as<int>();

    // Print the configuration for debugging
    Serial.printf("Settings received:\nCustomer Name: %s\nBLE HID Address: %s\nBase LED index: %d\n",
                  devConfig.customerName.c_str(),
                  devConfig.bleHidAddress.toString().c_str(),
                  devConfig.baseLedId);
}