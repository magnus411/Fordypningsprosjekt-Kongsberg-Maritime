#ifndef MQTT_SUBSCRIBER_H
#define MQTT_SUBSCRIBER_H
#include <src/Sdb.h>

sdb_errno MQTTSubscriber(const char *Address, const char *ClientId, const char *Topic);

#endif
