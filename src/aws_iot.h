#ifndef AWS_IOT_H
#define AWS_IOT_H

#include <PubSubClient.h>

void initAWS();
void messageHandler(char *topic, byte *payload, unsigned int length);

extern PubSubClient client;

#endif // AWS_IOT_H