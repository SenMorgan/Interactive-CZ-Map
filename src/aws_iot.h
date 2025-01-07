#ifndef AWS_IOT_H
#define AWS_IOT_H

#include <PubSubClient.h>

void initAWS(const char *id, size_t idLength);
void maintainAWSConnection();
void periodicStatusPublish();
void broadcastLedBlink(uint8_t ledId, String colorHex, uint16_t duration, uint16_t count = 1);

#endif // AWS_IOT_H