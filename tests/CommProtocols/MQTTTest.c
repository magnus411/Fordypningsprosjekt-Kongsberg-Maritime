#include <MQTTClient.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <src/Sdb.h>
SDB_LOG_REGISTER(MQTTTest);

#include <src/CommProtocols/MQTT.h>
#include <src/Common/CircularBuffer.h>

#define ADDRESS  "tcp://localhost:1883"
#define CLIENTID "ModbusPub"
#define TOPIC    "MODBUS"
#define QOS      1
#define TIMEOUT  10000L

// TODO(ingar): Is this right? Is there only ever one token?
volatile MQTTClient_deliveryToken DeliveredToken;

#if 0
static void
GeneratePowerShaftData_(u8 *DataBuffer)
{
    // Simulated power shaft data (32-bit values split into two 16-bit registers)
    u32 Power  = rand() % 1000 + 100; //  Power in kW (random between 100 and 1000)
    u32 Torque = rand() % 500 + 50;   // Torque in Nm (random between 50 and 550)
    u32 Rpm    = rand() % 5000 + 500; // RPM (random between 500 and 5500)

    // Store data in 16-bit register format (big-endian)
    DataBuffer[0] = (Power >> 8) & 0xFF;  // Power high byte
    DataBuffer[1] = Power & 0xFF;         // Power low byte
    DataBuffer[2] = (Torque >> 8) & 0xFF; // Torque high byte
    DataBuffer[3] = Torque & 0xFF;        // Torque low byte
    DataBuffer[4] = (Rpm >> 8) & 0xFF;    // RPM high byte
    DataBuffer[5] = Rpm & 0xFF;           // RPM low byte
}

void
GenerateModbusTcpFrame_(u8 *Buffer, u16 TransactionId, u16 ProtocolId, u16 Length, u8 UnitId,
                        u16 FunctionCode, u8 *Data, u16 DataLength)
{
    Buffer[0] = (TransactionId >> 8) & 0xFF;
    Buffer[1] = TransactionId & 0xFF;
    Buffer[2] = (ProtocolId >> 8) & 0xFF;
    Buffer[3] = ProtocolId & 0xFF;
    Buffer[4] = (Length >> 8) & 0xFF;
    Buffer[5] = Length & 0xFF;
    Buffer[6] = UnitId;
    Buffer[7] = FunctionCode;
    Buffer[8] = DataLength;
    memcpy(&Buffer[9], Data, DataLength);
}

void
TokenDelivered(void *Context, MQTTClient_deliveryToken Dt)
{
    SdbLogDebug("Message with token value %d delivery confirmed", Dt);
    DeliveredToken = Dt;
}

int
MsgArrivedServer(void *Context, char *TopicName, int TopicLen, MQTTClient_message *Message)
{
    SdbLogDebug("Message arrived\n     topic: %s\n   message: %.*s", TopicName, Message->payloadlen,
                (char *)Message->payload);
    MQTTClient_freeMessage(&Message);
    MQTTClient_free(TopicName);
    return 1;
}

void
ServerConnLost(void *Context, char *Cause)
{
    SdbLogDebug("\nConnection lost\n     cause: %s", Cause);
}

sdb_errno
MQTTPublisher(int ArgCount, char **ArgV)
{
    if(ArgCount < 2) {
        SdbLogError("Usage: %s <Rate (Hz)>", ArgV[0]);
        return -EINVAL;
    }

    int RateHz = atoi(ArgV[1]);
    if(RateHz <= 0) {
        SdbLogError("Invalid rate. Must be greater than 0.");
        return -EINVAL;
    }

    u64 DelayMicroseconds = 1000000 / RateHz;

    MQTTClient                Client;
    MQTTClient_connectOptions ConnOpts = MQTTClient_connectOptions_initializer;
    MQTTClient_message        PubMsg   = MQTTClient_message_initializer;
    MQTTClient_deliveryToken  Token;
    int                       Rc;

    MQTTClient_create(&Client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    ConnOpts.keepAliveInterval = 20;
    ConnOpts.cleansession      = 1;

    MQTTClient_setCallbacks(Client, NULL, ServerConnLost, MsgArrivedServer, TokenDelivered);
    if((Rc = MQTTClient_connect(Client, &ConnOpts)) != MQTTCLIENT_SUCCESS) {
        SdbLogError("Failed to connect, return code %d", Rc);
        return -1;
    }

    uint8_t DataBuffer[6];
    uint8_t ModbusFrame[256];

    while(1) {
        GeneratePowerShaftData_(DataBuffer);
        GenerateModbusTcpFrame_(ModbusFrame, 1, 0, 9, 1, 3, DataBuffer, 6);

        PubMsg.payload    = ModbusFrame;
        PubMsg.payloadlen = 15; // 9 bytes for Modbus frame header + 6 bytes of data
        PubMsg.qos        = QOS;
        PubMsg.retained   = 0;

        DeliveredToken = 0;
        MQTTClient_publishMessage(Client, TOPIC, &PubMsg, &Token);
        SdbLogDebug("Published Modbus frame on topic %s", TOPIC);

        // TODO(ingar): Since it's test code, a spinlock might be fine, but they should generally be
        // avoided
        while(DeliveredToken != Token)
            ;

        usleep(DelayMicroseconds);
    }

    MQTTClient_disconnect(Client, TIMEOUT);
    MQTTClient_destroy(&Client);

    return Rc;
}

sdb_errno
MqttInitTest(comm_protocol_api *Mqtt)
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
MqttRunTest(comm_protocol_api *Mqtt)
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
MqttFinalizeTest(comm_protocol_api *Mqtt)
{
    SdbArenaClear(&Mqtt->Arena);
    return 0;
}
/*
static circular_buffer Cb = { 0 };
sdb_errno
MQTTSubscriber(const char *Address, const char *ClientId, const char *Topic)
{
    CbInit(&Cb, SdbMebiByte(16), NULL);

    mqtt_ctx MqttSubscriber;
    InitSubscriber(&MqttSubscriber, "tcp://localhost:1883", "ModbusSub", "MODBUS", 1, &Cb);

    pthread_t MqttTid;
    pthread_create(&MqttTid, NULL, MQTTStartComm, &MqttSubscriber);
    pthread_join(MqttTid, NULL);

    CbFree(&Cb);

    return 0;
}
*/

#else
sdb_errno
MqttInitTest(comm_protocol_api *Mqtt)
{
    return 0;
}
sdb_errno
MqttFinalizeTest(comm_protocol_api *Mqtt)
{
    return 0;
}

sdb_errno
MqttRunTest(comm_protocol_api *Mqtt)
{
    return 0;
}

#endif
