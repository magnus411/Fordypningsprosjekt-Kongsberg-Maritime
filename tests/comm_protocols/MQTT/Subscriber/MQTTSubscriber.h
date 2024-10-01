#ifndef MQTT_PUBLISHER_H
#define MQTT_PUBLISHER_H
#include <SdbExtern.h>

sdb_errno MQTTSubscriber(const char *Address, const char *ClientId, const char *Topic);

#endif