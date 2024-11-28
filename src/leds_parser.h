#ifndef LEDS_PARSER_H
#define LEDS_PARSER_H

#include <ArduinoJson.h>
#include "leds.h"

/**
 * @brief Parses the LED configurations from a JSON document and sets the LEDs accordingly.
 *
 * @param doc The JSON document containing LED configurations.
 */
void setLedsFromJsonDoc(JsonDocument &doc);

#endif // LEDS_PARSER_H