#ifndef MODBUS_H
#define MODBUS_H

#include <src/Sdb.h>

SDB_BEGIN_EXTERN_C

#include <src/CommProtocols/CommProtocols.h>
#include <src/Common/SensorDataPipe.h>
#include <src/Common/Thread.h>

// NOTE(ingar): This is port used by modbus according to its wikipedia page
#define MODBUS_PORT (54321) // (502)

typedef struct
{
    sensor_data_pipe *Pipe;

    sdb_thread_control *Control;

    int        Port;
    int        SockFd;
    sdb_string Ip; // NOTE(ingar): Keep string last so it's allocated contiguously with the context

} mb_thread_ctx;

typedef struct
{
    sdb_thread         *Threads;
    sdb_thread_control *ThreadControls;
    mb_thread_ctx     **ThreadContexts;

} modbus_ctx;

typedef struct
{
    sdb_string *Ips;
    int        *Ports;

} mb_init_args;

#define MB_CTX(mb) ((modbus_ctx *)mb->Ctx)

void      MbThreadArenasInit(void);
ssize_t   MbReceiveTcpFrame(int Sockfd, u8 *Buffer, size_t BufferSize);
sdb_errno MbParseTcpFrame(const u8 *Buffer, int NumBytes, queue_item *Item);

sdb_errno MbPrepareThreads(comm_protocol_api *Mb);
sdb_errno MbInit(comm_protocol_api *Modbus);
sdb_errno MbRun(comm_protocol_api *Modbus);
sdb_errno MbFinalize(comm_protocol_api *Modbus);
sdb_errno MbSensorThread(sdb_thread *Thread);

SDB_END_EXTERN_C

#endif
