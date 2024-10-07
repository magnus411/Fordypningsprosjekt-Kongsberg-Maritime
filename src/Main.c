#include <stdlib.h>
#include <libpq-fe.h>

#define SDB_LOG_LEVEL 4
#include <Sdb.h>

SDB_LOG_REGISTER(Main);

#include <common/CircularBuffer.h>
#include <comm_protocols/CommProtocols.h>

int
main(int ArgCount, char **ArgV)
{

    circular_buffer Cb;
    CbInit(&Cb, 16000);

    comm_protocol_api ProtocolModbus;
    protocol_args     Args
        = { .Type = PROTOCOL_MODBUS_TCP, .Address = "127.0.0.1", .Port = 3490, .Cb = &Cb };

    PickProtocol(PROTOCOL_MODBUS_TCP, &ProtocolModbus);
    ProtocolModbus.Initialize(&ProtocolModbus, &Args);
    ProtocolModbus.StartComm(&ProtocolModbus);

    comm_protocol_api ProtocolMQTT;
    protocol_args     ArgsMQTT = { .Type    = PROTOCOL_MQTT,
                                   .Address = "tcp://127.0.0.1:1883",
                                   .Topic   = "MODBUS",
                                   .Qos     = 0,
                                   .Cb      = &Cb };

    PickProtocol(PROTOCOL_MQTT, &ProtocolMQTT);
    ProtocolMQTT.Initialize(&ProtocolMQTT, &ArgsMQTT);

    ProtocolMQTT.StartComm(&ProtocolMQTT);

    while(1) {
        sleep(1);
    }

    return 0;
}
