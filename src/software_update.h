#ifndef SOFTWARE_UPDATE_H
#define SOFTWARE_UPDATE_H

typedef void (*PublishResult)(bool success, const char *message);

void performFirmwareUpdate(const char *firmwareUrl, PublishResult publishResult);

#endif // SOFTWARE_UPDATE_H