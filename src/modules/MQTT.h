#ifndef MQTT_H
#define MQTT_H

#include "MQTTClient.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "../CircularBuffer.h"
#include "Modbus.h"

#define MAX_MODBUS_TCP_FRAME 260

//! Remember to use -lpaho-mqtt3c flag for compiling!
extern volatile MQTTClient_deliveryToken deliveredtoken;

typedef struct
{
    char            *Address;
    char            *ClientId;
    char            *Topic;
    int              Qos;
    long             Timeout;
    circular_buffer *Cb;
} mqtt_args;

typedef struct
{
    char            *Address;
    char            *ClientId;
    char            *Topic;
    int              Qos;
    long             Timeout;
    circular_buffer *Cb;
} MQTTSubscriber;

void  InitSubscriber(MQTTSubscriber *Sub, const char *Address, const char *ClientId,
                     const char *Topic, int Qos, circular_buffer *Cb);
void  ConnLost(void *context, char *cause);
int   MsgArrived(void *context, char *topicName, int topicLen, MQTTClient_message *message);
void *MQTTSubscriberThread(void *arg);

#endif // MQTT_H