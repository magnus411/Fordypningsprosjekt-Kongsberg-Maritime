#ifndef MODBUS_H
#define MODBUS_H

#include <src/Sdb.h>

SDB_BEGIN_EXTERN_C

#include <src/CommProtocols/CommProtocols.h>
#include <src/Common/SensorDataPipe.h>
#include <src/Common/Thread.h>

// NOTE(ingar): This is port used by modbus according to its wikipedia page
#define MODBUS_PORT (54321) // (502)
//
typedef struct
{
    u64 ConnCount;

    int        *SockFds;
    int        *Ports;
    sdb_string *Ips;
    // NOTE(ingar): Keep string last so it's allocated contiguously with the context

} modbus_ctx;

typedef struct
{
    u64  PortCount;
    int *Ports;

    u64         IpCount;
    sdb_string *Ips;

} mb_init_args;

#define MB_CTX(mb) ((modbus_ctx *)mb->Ctx)

void      MbThreadArenasInit(void);
ssize_t   MbReceiveTcpFrame(int Sockfd, u8 *Buffer, size_t BufferSize);
sdb_errno MbParseTcpFrame(const u8 *Buffer, int NumBytes, queue_item *Item);

sdb_errno MbPrepare(comm_protocol_api *Mb);
sdb_errno MbMainLoop(comm_protocol_api *Mb);

SDB_END_EXTERN_C

#endif
