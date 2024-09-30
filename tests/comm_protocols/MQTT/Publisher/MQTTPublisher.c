#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "MQTTClient.h"
#include <time.h>

#define SDB_LOG_LEVEL 4
#include "../../../../src/SdbExtern.h"
SDB_LOG_REGISTER(MQTT);

#define ADDRESS  "tcp://localhost:1883"
#define CLIENTID "ModbusPub"
#define TOPIC    "MODBUS"
#define QOS      1
#define TIMEOUT  10000L

//! Remember to use flag -lpaho-mqtt3c when compiling and have it installed

volatile MQTTClient_deliveryToken deliveredtoken;

void
GeneratePowerShaftData(uint8_t *DataBuffer)
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
GenerateModbusTcpFrame(u8 *Buffer, u16 TransactionId, u16 ProtocolId, u16 Length, u8 UnitId,
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
Delivered(void *Context, MQTTClient_deliveryToken Dt)
{
    SdbLogDebug("Message with token value %d delivery confirmed\n", Dt);
    deliveredtoken = Dt;
}

int
MsgArrivedServer(void *Context, char *TopicName, int TopicLen, MQTTClient_message *Message)
{
    SdbLogDebug("Message arrived\n     topic: %s\n   message: %.*s\n", TopicName,
                Message->payloadlen, (char *)Message->payload);
    MQTTClient_freeMessage(&Message);
    MQTTClient_free(TopicName);
    return 1;
}

void
ConnLostServer(void *Context, char *Cause)
{
    SdbLogDebug("\nConnection lost\n     cause: %s\n", Cause);
}

int
MQTTPublisher(int argc, char *argv[])
{
    if(argc < 2)
    {
        SdbLogDebug("Usage: %s <Rate (Hz)>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int RateHz = atoi(argv[1]);
    if(RateHz <= 0)
    {
        SdbLogError("Invalid rate. Must be greater than 0.\n");
        return EXIT_FAILURE;
    }

    long delayMicroseconds = 1000000 / RateHz;

    MQTTClient                Client;
    MQTTClient_connectOptions ConnOpts = MQTTClient_connectOptions_initializer;
    MQTTClient_message        PubMsg   = MQTTClient_message_initializer;
    MQTTClient_deliveryToken  Token;
    int                       Rc;

    MQTTClient_create(&Client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    ConnOpts.keepAliveInterval = 20;
    ConnOpts.cleansession      = 1;

    MQTTClient_setCallbacks(Client, NULL, ConnLostServer, MsgArrivedServer, Delivered);
    if((Rc = MQTTClient_connect(Client, &ConnOpts)) != MQTTCLIENT_SUCCESS)
    {
        SdbLogDebug("Failed to connect, return code %d\n", Rc);
        return EXIT_FAILURE;
    }

    uint8_t DataBuffer[6];
    uint8_t ModbusFrame[256];

    while(1)
    {
        GeneratePowerShaftData(DataBuffer);

        GenerateModbusTcpFrame(ModbusFrame, 1, 0, 9, 1, 3, DataBuffer, 6);

        PubMsg.payload    = ModbusFrame;
        PubMsg.payloadlen = 15; // 9 bytes for Modbus frame header + 6 bytes of data
        PubMsg.qos        = QOS;
        PubMsg.retained   = 0;

        deliveredtoken = 0;
        MQTTClient_publishMessage(Client, TOPIC, &PubMsg, &Token);
        SdbLogDebug("Published Modbus frame on topic %s\n", TOPIC);

        while(deliveredtoken != Token)
            ;

        usleep(delayMicroseconds);
    }

    MQTTClient_disconnect(Client, TIMEOUT);
    MQTTClient_destroy(&Client);
    return Rc;
}