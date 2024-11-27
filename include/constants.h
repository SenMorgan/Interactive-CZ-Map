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
// MQTT Configuration
// ============================================================================

// Device ID could be the same as the Thing Name or a unique identifier
#define DEVICE_ID THINGNAME

// Base MQTT topic for the project
#define MQTT_BASE_TOPIC "interactive-cz-map"

// Full MQTT topics for subscribing and publishing
// Commands sent to the device
#define MQTT_SUB_TOPIC_LEDS   MQTT_BASE_TOPIC "/" DEVICE_ID "/commands/leds"
#define MQTT_SUB_TOPIC_UPDATE MQTT_BASE_TOPIC "/" DEVICE_ID "/commands/update"

// Status published by the device
#define MQTT_PUB_TOPIC_STATUS MQTT_BASE_TOPIC "/" DEVICE_ID "/status"

// Wildcard topics for broader subscriptions
#define MQTT_SUB_TOPIC_ALL_COMMANDS MQTT_BASE_TOPIC "/" DEVICE_ID "/commands/#"

// ============================================================================
// Hardware Configuration
// ============================================================================

// Count of LEDs on the map
#define LEDS_COUNT 72

// IO pins
#define LEDS_PIN GPIO_NUM_25

// ============================================================================
// LED Effect Configuration
// ============================================================================

// Maximum fade effect duration in milliseconds
#define MAX_FADE_DURATION 5000

// Maximum number of fade effect repeats
#define MAX_FADE_REPEATS 100

// ============================================================================
// Software Versioning
// ============================================================================

// Software version must be set via build flags, so here we just check if it's defined
#ifndef SOFTWARE_VERSION
#error "SOFTWARE_VERSION must be defined in build flags!"
#endif

#endif // _CONSTANTS_H