#ifndef LEDS_H
#define LEDS_H

#include "crgb.h" // Include CRGB type from FastLED library for LED colors

#define LOOP_INDEFINITELY -1 // Value of fadeCycles to loop indefinitely
#define CIRCLE_EFFECT_SLOW_FADE_DURATION 1000
#define CIRCLE_EFFECT_FAST_FADE_DURATION 300

void ledsTaskInit();
void setLed(uint8_t index, uint8_t brightness, uint16_t fadeDuration, int16_t fadeCycles, CRGB color, bool useFadeIn = true);
void circleLedEffect(CRGB color, uint16_t fadeDuration, int16_t fadeCycles);

#endif // LEDS_H