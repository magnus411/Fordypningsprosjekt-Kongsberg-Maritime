#ifndef COMM_PROTOCOLS_H
#define COMM_PROTOCOLS_H

#include <stdatomic.h>
#include <stddef.h>
#include <SdbExtern.h>
#include <common/CircularBuffer.h>

typedef enum
{
    PROTOCOL_MODBUS_TCP,
    PROTOCOL_MQTT,
} protocol_type;

typedef struct
{
    protocol_type    Type;
    const char      *Address;
    int              Port;
    const char      *Topic;
    int              Qos;
    circular_buffer *Cb;

} protocol_args;

typedef struct comm_protocol_api comm_protocol_api;
struct comm_protocol_api
{
    sdb_errno (*Initialize)(comm_protocol_api *ProtocolApi, protocol_args *Args);
    sdb_errno (*StartComm)(comm_protocol_api *ProtocolApi);
    sdb_errno (*Cleanup)(comm_protocol_api *ProtocolApi);

    atomic_bool IsInitialized;
    void       *Context;
};

sdb_errno PickProtocol(protocol_type Type, comm_protocol_api *ProtocolApi);

#endif // COMM_PROTOCOLS_H
