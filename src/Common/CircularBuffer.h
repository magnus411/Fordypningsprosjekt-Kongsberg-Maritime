#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <pthread.h>
#include <stdbool.h>
#include <sys/types.h>

#include <src/Sdb.h>

#include <src/Metrics.h>
SDB_BEGIN_EXTERN_C

#define MAX_DATA_LENGTH 260

// TODO(ingar): Calculate data length from schema and use that instead of a default size like here
typedef struct
{
    int    Protocol;
    int    UnitId;
    u8     Data[MAX_DATA_LENGTH];
    size_t DataLength;

} queue_item;

typedef struct
{
    u8    *Data;
    size_t DataSize;

    size_t Head;
    size_t Tail;
    size_t Count;
    bool   Full;

    // TODO(ingar): Replace with sdb variants
    pthread_mutex_t WriteLock;
    pthread_mutex_t ReadLock;
    pthread_cond_t  NotEmpty;
    pthread_cond_t  NotFull;

    int Fd;

} circular_buffer;

sdb_errno CbInit(circular_buffer *Cb, size_t Size, sdb_arena *Arena);
bool      CbIsFull(circular_buffer *Cb);
bool      CbIsEmpty(circular_buffer *Cb);
ssize_t   CbInsert(circular_buffer *Cb, void *Data, size_t Size);
ssize_t   CbRead(circular_buffer *Cb, void *Dest, size_t Size);
void      CbFree(circular_buffer *Cb);

SDB_END_EXTERN_C

#endif // CIRCULARBUFFER_H
