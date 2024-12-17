
/**
 * @file SensorDataPipe.h
 * @brief Lock-Free Data Pipeline
 *
 * Key Features:
 * - Lock-free buffer management
 * - Dynamic arena allocation
 * - Event-driven synchronization
 *
 *
 */


#ifndef SENSOR_DATA_PIPE_H
#define SENSOR_DATA_PIPE_H

#include <stdatomic.h>

#include <src/Sdb.h>

SDB_BEGIN_EXTERN_C

#include <src/Common/Thread.h>

// TODO(ingar): Some way of tagging buffers for priorities or similar
// TODO(ingar): We might want to add a flag for the endianness of the data


/**
 * @brief Sensor Data Pipeline Structure
 *
 * Manages a set of memory arenas for efficient,
 * zero-copy sensor data transmission between threads.
 */
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

/**
 * @brief Create a Sensor Data Pipeline
 *
 * Initializes a set of memory arenas with event-based
 * synchronization for inter-thread data transfer.
 *
 * @param BufCount Number of buffers in the pipeline
 * @param BufSize Size of each buffer arena
 * @param Arena Optional memory arena for allocation
 * @return sensor_data_pipe* Initialized pipeline or NULL on failure
 */
sensor_data_pipe *SdpCreate(u64 BufCount, u64 BufSize, sdb_arena *Arena);

/**
 * @brief Destroy Sensor Data Pipeline
 *
 * Closes event file descriptors and optionally frees memory.
 *
 * @param Pipe Pipeline to destroy
 * @param AllocatedWithArena Whether pipeline was arena-allocated
 */
void SdpDestroy(sensor_data_pipe *Pipe, bool AllocatedWithArena);

/**
 * @brief Acquire Write Buffer Arena
 *
 * Obtains the next available memory arena for writing,
 * blocking if no buffer is available.
 *
 * @param Pipe Pipeline instance
 * @return sdb_arena* Available write buffer arena or NULL
 */
sdb_arena *SdPipeGetWriteBuffer(sensor_data_pipe *Pipe);

/**
 * @brief Acquire Read Buffer Arena
 *
 * Obtains the next available memory arena for reading,
 * blocking until data is available.
 *
 * @param Pipe Pipeline instance
 * @return sdb_arena* Available read buffer arena or NULL
 */
sdb_arena *SdPipeGetReadBuffer(sensor_data_pipe *Pipe);


/**
 * @brief Flush Partial Buffer Arena
 *
 * Signals and transitions a partially filled buffer arena
 * to the read side of the pipeline.
 *
 * @param Pipe Pipeline instance
 */
void SdPipeFlush(sensor_data_pipe *Pipe);

SDB_END_EXTERN_C

#endif
