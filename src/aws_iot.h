#ifndef AWS_IOT_H
#define AWS_IOT_H

#include <PubSubClient.h>

void initAWS();
void maintainAWSConnection();
void periodicStatusPublish();

#endif // AWS_IOT_H