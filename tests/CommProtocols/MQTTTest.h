#ifndef MQTT_TEST_H
#define MQTT_TEST_H


#include <MQTTClient.h>

#include <src/Sdb.h>

#include <src/CommProtocols/CommProtocols.h>
#include <src/Common/SensorDataPipe.h>

// TODO(ingar): This doesn't seem right. Is there only ever one token? Also, this is not threadsafe
// at all.
extern volatile MQTTClient_deliveryToken DeliveredToken;

sdb_errno MQTTPublisher(int ArgCount, char **ArgV);
sdb_errno MQTTSubscriber(const char *Address, const char *ClientId, const char *Topic);

sdb_errno MqttInitTest(comm_protocol_api *MQTT);
sdb_errno MqttRunTest(comm_protocol_api *MQTT);
sdb_errno MqttFinalizeTest(comm_protocol_api *MQTT);


#endif
