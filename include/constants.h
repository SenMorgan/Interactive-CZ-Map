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
#define FIRMWARE_VERSION "1.1.0"

// Timeout for detecting double reset (used to manually enter Config Portal)
#define DRD_TIMEOUT 2000

// ============================================================================
// MQTT Configuration
// ============================================================================

// Base MQTT topic for the project
#define MQTT_BASE_TOPIC "int-cz-map"

// Commands sent to the device. Could be followed by the client ID
// to target a specific device.
#define MQTT_SUB_TOPIC_LEDS   MQTT_BASE_TOPIC "/cmd/leds"
#define MQTT_SUB_TOPIC_UPDATE MQTT_BASE_TOPIC "/cmd/update"

// Topics published by the device followed by the client ID
#define MQTT_PUB_TOPIC_STATUS        MQTT_BASE_TOPIC "/status/device"
#define MQTT_PUB_TOPIC_UPDATE_STATUS MQTT_BASE_TOPIC "/status/update"

// ============================================================================
// LED Effect Configuration
// ============================================================================

// Maximum and minimum fade effect duration in milliseconds
#define MAX_FADE_DURATION 5000
#define MIN_FADE_DURATION 200

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