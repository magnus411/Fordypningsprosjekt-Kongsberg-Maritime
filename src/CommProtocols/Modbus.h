#ifndef MODBUS_H
#define MODBUS_H

#include <src/Sdb.h>

#include <src/CommProtocols/CommProtocols.h>
#include <src/Common/SensorDataPipe.h>

// NOTE(ingar): This is port used by modbus according to its wikipedia page
#define MODBUS_PORT (3490) // (502)

typedef struct
{
    int  PORT;
    char Ip[10];
    int  SockFd;

} modbus_ctx;

#define MB_CTX(mb) ((modbus_ctx *)mb->Ctx)

ssize_t   RecivedModbusTCPFrame(int Sockfd, u8 *Buffer, size_t BufferSize);
sdb_errno ParseModbusTCPFrame(const u8 *Buffer, int NumBytes, queue_item *Item);

sdb_errno ModbusInit(comm_protocol_api *Modbus);
sdb_errno ModbusRun(comm_protocol_api *Modbus);
sdb_errno ModbusFinalize(comm_protocol_api *Modbus);

#endif
