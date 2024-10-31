#ifndef SENSOR_DATA_PIPE_H
#define SENSOR_DATA_PIPE_H

#include <stdatomic.h>

#include <src/Sdb.h>

SDB_BEGIN_EXTERN_C

#include <src/Common/CircularBuffer.h>
#include <src/Common/Thread.h>

// TODO(ingar): Some way of tagging buffers for priorities or similar
typedef struct
{
    // circular_buffer *Buffers;
    size_t BufferMaxFill;  // It's unlikely that a given buffer can be filled exactly
    size_t PacketSize;     // Filled by db during init
    u64    PacketMaxCount; // --||--

    atomic_uint WriteBufIdx;
    atomic_uint ReadBufIdx;
    atomic_uint FullBuffersCount;

    int ReadEventFd;
    int WriteEventFd;

    u64         BufCount;
    sdb_arena **Buffers;

} sensor_data_pipe;

sensor_data_pipe **SdPipesInit(u64 PipeCount, u64 BufCount, size_t BufSize, sdb_arena *Arena);
sdb_arena         *SdPipeGetWriteBuffer(sensor_data_pipe *Pipe);
sdb_arena         *SdPipeGetReadBuffer(sensor_data_pipe *Pipe);
void               SdPipeFlush(sensor_data_pipe *Pipe);

SDB_END_EXTERN_C

#endif
