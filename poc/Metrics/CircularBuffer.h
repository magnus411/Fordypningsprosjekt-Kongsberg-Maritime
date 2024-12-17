

/**
 * @file CircularBuffer.h
 * @brief Header for Thread-Safe Circular Buffer Implementation
 *
 * Provides a flexible, thread-safe circular buffer data structure
 * designed for high-performance, concurrent data streaming and
 * inter-thread communication.
 *
 *
 */

#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <stdbool.h>
#include <sys/types.h>

#include <src/Sdb.h>

#include <src/Common/Metrics.h>
#include <src/Common/Thread.h>

SDB_BEGIN_EXTERN_C

#define MAX_DATA_LENGTH 260

// TODO(ingar): Calculate data length from schema and use that instead of a default size like here


/**
 * @brief Represents a single item in the queue
 *
 * Stores protocol, unit ID, and variable-length data
 */
typedef struct
{
    int    Protocol;
    int    UnitId;
    u8     Data[MAX_DATA_LENGTH];
    size_t DataLength;

} queue_item;


/**
 * @brief  Circular Buffer Structure
 *
 * Implements a ring buffer with concurrent access controls
 * and performance metrics tracking.
 */
typedef struct
{
    u8    *Data;
    size_t DataSize;

    size_t Head;
    size_t Tail;
    size_t Count;
    bool   Full;

    // TODO(ingar): Replace with sdb variants
    sdb_mutex WriteLock;
    sdb_mutex ReadLock;
    sdb_cond  NotEmpty;
    sdb_cond  NotFull;


} circular_buffer;


/**
 * @brief Initialize a Circular Buffer
 *
 * Allocates memory and sets up synchronization primitives
 *
 * @param Cb Pointer to circular buffer
 * @param Size Buffer size in bytes
 * @param Arena Optional memory arena for allocation
 * @return sdb_errno 0 on success, error code on failure
 */
sdb_errno CbInit(circular_buffer *Cb, size_t Size, sdb_arena *Arena);

/**
 * @brief Check if Circular Buffer is Full
 *
 * @param Cb Pointer to circular buffer
 * @return bool True if buffer is full, false otherwise
 */
bool CbIsFull(circular_buffer *Cb);

/**
 * @brief Check if Circular Buffer is Empty
 *
 * @param Cb Pointer to circular buffer
 * @return bool True if buffer is empty, false otherwise
 */
bool CbIsEmpty(circular_buffer *Cb);

/**
 * @brief Insert Data into Circular Buffer
 *
 * Performs a thread-safe write operation, blocking if buffer is full
 *
 * @param Cb Pointer to circular buffer
 * @param Data Pointer to data to be inserted
 * @param Size Size of data to insert
 * @return ssize_t Number of bytes inserted
 */
ssize_t CbInsert(circular_buffer *Cb, void *Data, size_t Size);


/**
 * @brief Read Data from Circular Buffer
 *
 * Performs a thread-safe read operation, blocking if insufficient data
 *
 * @param Cb Pointer to circular buffer
 * @param Dest Destination buffer for read data
 * @param Size Number of bytes to read
 * @return ssize_t Number of bytes read
 */
ssize_t CbRead(circular_buffer *Cb, void *Dest, size_t Size);

/**
 * @brief Free Circular Buffer Resources
 *
 * Releases synchronization primitives
 * @note Does not free underlying data memory
 *
 * @param Cb Pointer to circular buffer
 */
void CbFree(circular_buffer *Cb);

SDB_END_EXTERN_C

#endif // CIRCULARBUFFER_H
