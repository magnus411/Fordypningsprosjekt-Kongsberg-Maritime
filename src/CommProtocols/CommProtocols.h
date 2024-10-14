#ifndef COMM_PROTOCOLS_H
#define COMM_PROTOCOLS_H
#include <stdatomic.h>
#include <stddef.h>

#include <src/Common/CircularBuffer.h>

typedef enum
{
    Protocol_Modbus_TCP,
    Protocol_MQTT
} Protocol_Type;

typedef struct comm_protocol_api comm_protocol_api;
struct comm_protocol_api
{
    sdb_errno (*Initialize)(comm_protocol_api *Protocol, void *Args);
    void *(*StartComm)(void *Protocol);
    sdb_errno (*Cleanup)(comm_protocol_api *Protocol);

    atomic_bool IsInitialized;
    void       *Context;
};

sdb_errno PickProtocol(Protocol_Type Type, comm_protocol_api *Protocol);

#endif // COMM_PROTOCOLS_H
