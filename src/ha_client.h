#ifndef HA_CLIENT_H
#define HA_CLIENT_H

#include <PubSubClient.h>
#include <WiFiClient.h>

// Available only when USE_HOME_ASSISTANT is defined
void haClientTaskInit(char *clientId, size_t idLength);

// Always available. Returns true when USE_HOME_ASSISTANT is not defined
bool isMapEnabled();

// Variables used in status publishing
extern uint32_t awsReconnectAttempts;
extern uint32_t awsMsgsReceived;

#endif // HA_CLIENT_H