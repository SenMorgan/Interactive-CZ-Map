#ifndef LEDS_H
#define LEDS_H

#include <FastLED.h>

void initLeds();
void setLed(int index, int brightness, int fadeDuration, int fadeCycles, CRGB color);
void refreshLeds();

#endif // LEDS_H