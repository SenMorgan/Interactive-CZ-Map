#ifndef LEDS_H
#define LEDS_H

#include "crgb.h" // Include CRGB type from FastLED library for LED colors

#define LOOP_INDEFINITELY                -1 // Value of fadeCycles to loop indefinitely
#define CIRCLE_EFFECT_SLOW_FADE_DURATION 1000
#define CIRCLE_EFFECT_FAST_FADE_DURATION 300

// Structure for LED commands
struct LedCommand
{
    uint8_t brightness;    // Brightness level (0-255)
    uint16_t fadeDuration; // Duration of the fade effect in milliseconds
    int16_t fadeCycles;    // Number of times to perform the effect (-1 for infinite)
    CRGB color;            // Color of the LED
};

void ledsTaskInit();
void startProgressIndication();
void stopProgressIndication();
void progressIndicator(uint8_t progress, CRGB color);
void pushLedCommand(uint8_t index, LedCommand command);
void circleLedEffect(CRGB color, uint16_t fadeDuration, int16_t fadeCycles);
void blinkWithSingleLed(uint8_t index, CRGB color, uint16_t fadeDuration, int16_t fadeCycles);

#endif // LEDS_H