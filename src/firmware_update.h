#ifndef FIRMWARE_UPDATE_H
#define FIRMWARE_UPDATE_H

#include <crgb.h> // Include CRGB type from FastLED library for LED colors

typedef void (*PublishResult)(bool success, const char *message);

void performFirmwareUpdate(const char *firmwareUrl, PublishResult publishResult);

#endif // FIRMWARE_UPDATE_H