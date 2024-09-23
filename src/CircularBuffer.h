#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

#define MAX_DATA_LENGTH 260

typedef struct
{
    int     Protocol;
    int     UnitId;
    uint8_t Data[MAX_DATA_LENGTH];
    size_t  DataLength;
} QueueItem;

typedef struct
{
    void           *Buffer;
    size_t          Size;
    size_t          Head;
    size_t          Tail;
    size_t          Count;
    int             Full;
    pthread_mutex_t WriteLock;
    pthread_mutex_t ReadLock;
    pthread_cond_t  NotEmpty;
    pthread_cond_t  NotFull;
} CircularBuffer;

void   InitCircularBuffer(CircularBuffer *Cb, size_t Size);
int    IsFull(CircularBuffer *Cb);
int    IsEmpty(CircularBuffer *Cb);
size_t InsertToBuffer(CircularBuffer *Cb, void *Data, size_t Size);
size_t ReadFromBuffer(CircularBuffer *Cb, void *Dest, size_t Size);
void   FreeCircularBuffer(CircularBuffer *Cb);

#endif // CIRCULARBUFFER_H