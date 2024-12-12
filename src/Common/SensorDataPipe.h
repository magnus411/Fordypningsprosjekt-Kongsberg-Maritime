#ifndef SENSOR_DATA_PIPE_H
#define SENSOR_DATA_PIPE_H

#include <stdatomic.h>

#include <src/Sdb.h>

SDB_BEGIN_EXTERN_C

#include <src/Common/CircularBuffer.h>
#include <src/Common/Thread.h>

// TODO(ingar): Some way of tagging buffers for priorities or similar
// TODO(ingar): We might want to add a flag for the endianness of the data
typedef struct
{
    size_t BufferMaxFill; // It's unlikely that a given buffer can be filled exactly
    size_t PacketSize;    // Filled by db during init
    u64    ItemMaxCount;  // --||--

    atomic_uint WriteBufIdx;
    atomic_uint ReadBufIdx;
    atomic_uint FullBuffersCount;

    int ReadEventFd;
    int WriteEventFd;

    u64         BufCount;
    sdb_arena **Buffers;

} sensor_data_pipe;

sensor_data_pipe *SdpCreate(u64 BufCount, u64 BufSize, sdb_arena *Arena);
void              SdpDestroy(sensor_data_pipe *Pipe, bool AllocatedWithArena);
sdb_arena        *SdPipeGetWriteBuffer(sensor_data_pipe *Pipe, sdb_timediff TimeoutMs);
sdb_arena        *SdPipeGetReadBuffer(sensor_data_pipe *Pipe, sdb_timediff TimeoutMs);
void              SdPipeFlush(sensor_data_pipe *Pipe, sdb_timediff TimeoutMs);
SDB_END_EXTERN_C

#endif
