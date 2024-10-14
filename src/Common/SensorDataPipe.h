#ifndef SENSOR_DATA_PIPE_H
#define SENSOR_DATA_PIPE_H

#include <src/Common/CircularBuffer.h>

// TODO(ingar): Some way of tagging buffers for priorities or similar
typedef struct
{
    i64              BufCount;
    size_t          *BufSizes;
    circular_buffer *Buffers;

} sensor_data_pipe;

sdb_errno SdPipeInit(sensor_data_pipe *SdPipe, u64 BufCount, size_t BufSizes[], sdb_arena *Arena);

#endif
