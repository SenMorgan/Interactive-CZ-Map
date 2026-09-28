#ifndef HA_CLIENT_H
#define HA_CLIENT_H

#include <PubSubClient.h>
#include <WiFiClient.h>

void haClientTaskInit(char *clientId, size_t idLength);
bool isMapEnabled();

// Variables used in status publishing
extern uint32_t awsReconnectAttempts;
extern uint32_t awsMsgsReceived;

#endif // HA_CLIENT_H