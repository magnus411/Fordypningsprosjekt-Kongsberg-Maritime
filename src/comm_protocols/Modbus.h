#include <stdint.h>

#include <SdbExtern.h>
#include <common/CircularBuffer.h>
#include <comm_protocols/CommProtocols.h>
#ifndef MODBUSMODULE_H
#define MODBUSMODULE_H

typedef struct
{
    int    Protocol;
    int    UnitId;
    u8     Data[MAX_DATA_LENGTH];
    size_t DataLength;
} queue_item;

typedef struct
{
    int              PORT;
    char             Ip[10];
    circular_buffer *Cb;

} modbus_args;

sdb_errno ModbusInitialize(comm_protocol_api *Modbus, void *Args);

void *ModbusStartComm(void *Modbus);

sdb_errno ModbusCleanup(comm_protocol_api *Modbus);

#endif /* MODBUSMODULE_H */
