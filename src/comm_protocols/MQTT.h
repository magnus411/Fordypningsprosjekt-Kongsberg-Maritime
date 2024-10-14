#ifndef MQTT_H
#define MQTT_H

#include <MQTTClient.h>

#include <SdbExtern.h>
#include <common/CircularBuffer.h>
#include <comm_protocols/Modbus.h>
#include <comm_protocols/CommProtocols.h>

// TODO(ingar): This doesn't seem right. Is there only ever one token? Also, this is not threadsafe
// at all.
extern volatile MQTTClient_deliveryToken DeliveredToken;

typedef struct
{
    char            *Address;
    char            *ClientId;
    char            *Topic;
    int              Qos;
    long             Timeout;
    circular_buffer *Cb;
} mqtt_args;

sdb_errno InitSubscriber(mqtt_args *Sub, const char *Address, const char *ClientId,
                         const char *Topic, int Qos, circular_buffer *Cb);
void      ConnLost(void *Context, char *Cause);
sdb_errno MsgArrived(void *Context, char *TopicName, int TopicLen, MQTTClient_message *Message);

sdb_errno MQTTInitialize(comm_protocol_api *MQTT, void *Args);
void     *MQTTStartComm(void *MQTT);
sdb_errno MQTTCleanup(comm_protocol_api *MQTT);

#endif // MQTT_H
