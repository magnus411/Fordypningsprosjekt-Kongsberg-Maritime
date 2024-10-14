#ifndef MODBUSMODULE_H
#define MODBUSMODULE_H

#include <src/Sdb.h>

#include <src/CommProtocols/CommProtocols.h>
#include <src/Common/CircularBuffer.h>

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
