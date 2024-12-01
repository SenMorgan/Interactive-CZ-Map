#ifndef LEDS_H
#define LEDS_H

#include "crgb.h" // Include CRGB type from FastLED library for LED colors

#define LOOP_INDEFINITELY -1 // Value of fadeCycles to loop indefinitely

void ledsTaskInit();
void setLed(uint8_t index, uint8_t brightness, uint16_t fadeDuration, int16_t fadeCycles, CRGB color, bool useFadeIn = true);
void circleLedEffect(CRGB color);

#endif // LEDS_H