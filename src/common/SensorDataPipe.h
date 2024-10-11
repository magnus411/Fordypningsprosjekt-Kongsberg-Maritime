#ifndef SENSOR_DATA_PIPE_H
#define SENSOR_DATA_PIPE_H

#include <common/CircularBuffer.h>

typedef struct
{
    circular_buffer *CircularBuffers;
} sd_pipe;

#endif
