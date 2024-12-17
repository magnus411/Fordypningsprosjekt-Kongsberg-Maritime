/**
 * @file MQTT.c
 * @brief MQTT protocol implementation (Experimental)
 * @warning This code is currently disabled and under development
 */

#include "src/Common/SensorDataPipe.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <MQTTClient.h>

#include <src/Sdb.h>

SDB_LOG_REGISTER(MQTT);

#include <src/CommProtocols/MQTT.h>
#include <src/CommProtocols/Modbus.h>
#include <src/Common/CircularBuffer.h>

#define MAX_MODBUS_TCP_FRAME  260
#define MODBUS_TCP_HEADER_LEN 7

// WARN: Callback function, do not change signature!
void
ConnLost(void *Ctx, char *Cause)
{
    SdbLogError("Connection lost. Cause: %s", Cause);
}

// WARN: Callback function, do not change signature! Do not free message and topic name when
// returning 0!
int
MsgArrived(void *Ctx_, char *TopicName, int TopicLen, MQTTClient_message *Message)
{
    mqtt_ctx *Ctx = Ctx_;
    SdbLogDebug("Message arrived. Topic: %s", TopicName);

    if(Message->payloadlen > MAX_MODBUS_TCP_FRAME) {
        SdbLogWarning("Message too large to store in buffer. Skipping...");
        goto exit; // TODO(ingar): Returning 0 will make the library run the function again, which
                   // is pointless in this case
    }

    u8        *Buffer = (u8 *)Message->payload;
    queue_item Item;

    if(ParseModbusTCPFrame(Buffer, Message->payloadlen, &Item) == 0) {
        ssize_t BytesWritten = 0; // SdPipeInsert(Ctx->SdPipe, 0, Item.Data, Item.DataLength);
        if(BytesWritten > 0) {
            SdbLogDebug("Inserted Modbus data into buffer. Bytes written: %zd", BytesWritten);
        } else {
            SdbLogError("Failed to insert Modbus data into buffer.");
            return 0;
        }
    } else {
        SdbLogError("Failed to parse Modbus frame from MQTT message.");
        return 0;
    }

exit:
    MQTTClient_freeMessage(&Message);
    MQTTClient_free(TopicName);

    return 1;
}

sdb_errno
InitSubscriber(mqtt_ctx *Ctx, const char *Address, const char *ClientId, const char *Topic, int Qos,
               sensor_data_pipe *SdPipe, sdb_arena *Arena)
{
    return 0;
}

sdb_errno
MqttInit(comm_protocol_api *Mqtt)
{
    // TODO(ingar): Get mqtt config from file
    char ClientName[128] = { 0 };
    snprintf(ClientName, SdbArrayLen(ClientName), "CommT%ld", (i64)Mqtt->OptArgs);

    mqtt_ctx *MqttCtx   = SdbPushStruct(&Mqtt->Arena, mqtt_ctx);
    MqttCtx->Address    = SdbStrdup("tcp://localhost:1883", &Mqtt->Arena);
    MqttCtx->ClientName = SdbStrdup(ClientName, &Mqtt->Arena);
    MqttCtx->Topic      = SdbStrdup("SensorData", &Mqtt->Arena);
    MqttCtx->Qos        = 1;
    MqttCtx->Timeout    = 10000L;
    MqttCtx->SdPipe     = &Mqtt->SdPipe;

    Mqtt->Ctx = MqttCtx;
    return 0;
}

sdb_errno
MqttRun(comm_protocol_api *Mqtt)
{
    mqtt_ctx *Ctx = Mqtt->Ctx;

    MQTTClient                Client;
    MQTTClient_connectOptions ConnOptions = MQTTClient_connectOptions_initializer;
    int                       Rc;

    MQTTClient_create(&Client, Ctx->Address, Ctx->ClientName, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    ConnOptions.keepAliveInterval = 20;
    ConnOptions.cleansession      = 1;

    MQTTClient_setCallbacks(Client, Ctx, ConnLost, MsgArrived, NULL);

    if((Rc = MQTTClient_connect(Client, &ConnOptions)) != MQTTCLIENT_SUCCESS) {
        SdbLogError("Failed to connect to MQTT broker. Error: %d", Rc);
        return -1;
    }

    SdbLogDebug("Subscribing to topic %s for Client %s using QoS %d", Ctx->Topic, Ctx->ClientName,
                Ctx->Qos);
    MQTTClient_subscribe(Client, Ctx->Topic, Ctx->Qos);

    while(1) {
        // TODO(ingar): lmao. Add termination logic plz
        // Keep running until manually stopped or thread termination logic is added
    }

    MQTTClient_disconnect(Client, 10000);
    MQTTClient_destroy(&Client);

    return 0;
}

sdb_errno
MqttFinalize(comm_protocol_api *Mqtt)
{
    SdbArenaClear(&Mqtt->Arena);
    return 0;
}
