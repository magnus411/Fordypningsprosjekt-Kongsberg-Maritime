#ifndef MODBUS_H
#define MODBUS_H

#include <src/Sdb.h>

#include <src/CommProtocols/CommProtocols.h>
#include <src/Common/SensorDataPipe.h>

typedef struct
{
    int  PORT;
    char Ip[10];

} modbus_ctx;

sdb_errno ModbusInit(comm_protocol_api *Modbus, void *OptArgs);
sdb_errno ModbusRun(comm_protocol_api *Modbus);
sdb_errno ModbusFinalize(comm_protocol_api *Modbus);

#endif
