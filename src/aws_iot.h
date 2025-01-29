#ifndef AWS_IOT_H
#define AWS_IOT_H

#include <PubSubClient.h>

void initAWS(const char *id, size_t idLength);
void maintainAWSConnection();
void periodicStatusPublishAWS();

#endif // AWS_IOT_H