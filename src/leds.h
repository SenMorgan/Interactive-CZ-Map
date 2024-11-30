#ifndef LEDS_H
#define LEDS_H

#include <FastLED.h>

void ledsTaskInit();
void setLed(int index, int brightness, int fadeDuration, int fadeCycles, CRGB color);

#endif // LEDS_H