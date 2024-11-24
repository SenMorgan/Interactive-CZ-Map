#ifndef LEDS_H
#define LEDS_H

#include <FastLED.h>

void initLeds();
void blinkAllLeds();

extern CRGB leds[];

#endif // LEDS_H