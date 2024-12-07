/**
 * @file constants.h
 * @brief Defines constants and MQTT topic structure for the Interactive CZ Map project.
 *
 * @author SenMorgan https://github.com/SenMorgan
 * @date 2024-11-22
 *
 * @copyright Copyright (c) 2024 Sen Morgan
 *
 */

#ifndef _CONSTANTS_H
#define _CONSTANTS_H

#include "secrets.h"

// ============================================================================
// System Configuration
// ============================================================================

// Hostname format: <Hostname prefix>_<Chip ID>
#define HOSTNAME_PREFIX "Interactive-CZ-Map"

// Firmware version of the Interactive CZ Map project
#define FIRMWARE_VERSION "0.0.1"

// ============================================================================
// MQTT Configuration
// ============================================================================

// Maximum length of the client ID (extend if necessary)
#define MAX_CLIENT_ID_LENGTH 32

// Base MQTT topic for the project
#define MQTT_BASE_TOPIC "interactive-cz-map"

// General topics subscribed to by all devices
#define MQTT_SUB_TOPIC_LEDS_GENERAL   MQTT_BASE_TOPIC "/commands/leds"
#define MQTT_SUB_TOPIC_UPDATE_GENERAL MQTT_BASE_TOPIC "/commands/update"

// Device-specific topics subscribed to by the device
#define MQTT_SUB_TOPIC_LEDS_TEMPLATE   MQTT_BASE_TOPIC "/%s/commands/leds"
#define MQTT_SUB_TOPIC_UPDATE_TEMPLATE MQTT_BASE_TOPIC "/%s/commands/update"

// Device-specific topics published by the device
#define MQTT_PUB_TOPIC_STATUS_TEMPLATE        MQTT_BASE_TOPIC "/%s/status"
#define MQTT_PUB_TOPIC_UPDATE_STATUS_TEMPLATE MQTT_BASE_TOPIC "/%s/status/update"

// ============================================================================
// LED Effect Configuration
// ============================================================================

// Maximum fade effect duration in milliseconds
#define MAX_FADE_DURATION 5000

// Maximum number of fade effect repeats
#define MAX_FADE_REPEATS 100

// ============================================================================
// Hardware Configuration
// ============================================================================

// Count of LEDs on the map
#define LEDS_COUNT 72

// IO pins
#define LEDS_PIN GPIO_NUM_25

#endif // _CONSTANTS_H