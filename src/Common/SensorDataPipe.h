#ifndef SENSOR_DATA_PIPE_H
#define SENSOR_DATA_PIPE_H

#include <stdio.h>

#include <src/Common/CircularBuffer.h>

// TODO(ingar): Some way of tagging buffers for priorities or similar
typedef struct
{
    i64              BufCount;
    size_t          *BufSizes;
    circular_buffer *Buffers;

} sensor_data_pipe;

sdb_errno SdPipeInit(sensor_data_pipe *SdPipe, u64 BufCount, size_t BufSizes[], sdb_arena *Arena);
ssize_t   SdPipeInsert(sensor_data_pipe *SdPipe, u64 Buf, void *Data, size_t Size);
ssize_t   SdPipeRead(sensor_data_pipe *SdPipe, u64 Buf, void *To, size_t Size);

#endif
