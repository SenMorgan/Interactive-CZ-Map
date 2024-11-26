/**
 * @file constants.h
 * @author SenMorgan https://github.com/SenMorgan
 * @date 2024-11-22
 *
 * @copyright Copyright (c) 2024 Sen Morgan
 *
 */

#ifndef _CONSTANTS_H
#define _CONSTANTS_H

// Topic to subscribe on AWS IoT Core
#define AWS_IOT_SUBSCRIBE_TOPIC "interactive-cz-map"

// Count of LEDs on the map
#define LEDS_COUNT 72

// Maximum fade effect duration in milliseconds
#define MAX_FADE_DURATION 5000

// Maximum number of fade effect repeats
#define MAX_FADE_REPEATS 100

// IO pins
#define LEDS_PIN GPIO_NUM_25

#endif // _CONSTANTS_H