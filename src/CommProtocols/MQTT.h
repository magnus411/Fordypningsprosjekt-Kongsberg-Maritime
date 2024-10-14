#ifndef MQTT_H
#define MQTT_H

#include <MQTTClient.h>

#include <src/Sdb.h>

#include <src/CommProtocols/Modbus.h>
#include <src/Common/CircularBuffer.h>

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
int       MsgArrived(void *Context, char *TopicName, int TopicLen, MQTTClient_message *Message);
void     *MQTTSubscriberThread(void *Arg);

#endif // MQTT_H
