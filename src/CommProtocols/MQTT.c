#include "src/CommProtocols/CommProtocols.h"
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

/**
 * @resources: https://eclipse.dev/paho/files/mqttdoc/MQTTClient/html/index.html
 */
static sdb_errno
ParseModbusTCPFrame_(const u8 *Buffer, size_t NumBytes, queue_item *Item)
{
    if(NumBytes < MODBUS_TCP_HEADER_LEN) {
        SdbLogError("Invalid Modbus frame");
        return -EINVAL;
    }

    // Modbus TCP frame structure:
    // | Transaction ID | Protocol ID | Length | Unit ID | Function Code | DataLength | Data  |
    // | 2 bytes        | 2 bytes     | 2 bytes| 1 byte  | 1 byte        | 1 byte     | n bytes |
    // u16 TransactionId = (Buffer[0] << 8) | Buffer[1];
    // u16 ProtocolId    = (Buffer[2] << 8) | Buffer[3];
    // u16 Length        = (Buffer[4] << 8) | Buffer[5];
    u8 UnitId       = Buffer[6];
    u8 FunctionCode = Buffer[7];
    u8 DataLength   = Buffer[8];

    Item->UnitId   = UnitId;
    Item->Protocol = FunctionCode;

    // Handle function code 0x03: read multiple holding registers
    if(FunctionCode != 0x03) {
        SdbLogWarning("Unsupported function code: %u", FunctionCode);
        return -1;
    }

    SdbLogDebug("Byte Count: %u", DataLength);

    // Ensure the byte count is even and does not exceed the maximum allowed length
    if(DataLength % 2 != 0) {
        SdbLogWarning("Odd byte count detected. Skipping this frame.");
        return -1;
    }

    if(DataLength > MAX_MODBUS_TCP_FRAME - MODBUS_TCP_HEADER_LEN) {
        SdbLogWarning("Byte count exceeds maximum allowed frame size. Skipping this frame.");
        return -1;
    }

    // Copy only the Modbus data into the queue item
    memcpy(Item->Data, &Buffer[9], DataLength);
    Item->DataLength = DataLength;

    return 0;
}

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

    if(ParseModbusTCPFrame_(Buffer, Message->payloadlen, &Item) == 0) {
        ssize_t BytesWritten = SdPipeInsert(Ctx->SdPipe, 0, Item.Data, Item.DataLength);
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

static sdb_errno
InitSubscriber(mqtt_ctx *Ctx, const char *Address, const char *ClientId, const char *Topic, int Qos,
               sensor_data_pipe *SdPipe, sdb_arena *Arena)
{
    return 0;
}

sdb_errno
MqttInit(comm_protocol_api *Mqtt, void *OptArgs)
{
    // TODO(ingar): Get mqtt config from file
    char ClientName[128] = { 0 };
    snprintf(ClientName, SdbArrayLen(ClientName), "CommT%ld", (i64)OptArgs);

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
