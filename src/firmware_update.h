#ifndef FIRMWARE_UPDATE_H
#define FIRMWARE_UPDATE_H

typedef void (*PublishResult)(bool success, const char *message);

void performFirmwareUpdate(const char *firmwareUrl, PublishResult publishResult);

#endif // FIRMWARE_UPDATE_H