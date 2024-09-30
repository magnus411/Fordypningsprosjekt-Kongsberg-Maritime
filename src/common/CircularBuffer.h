#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>

#include <SdbExtern.h>

#define MAX_DATA_LENGTH 260

typedef struct
{
    u8             *Data;
    size_t          Size;
    size_t          Head;
    size_t          Tail;
    size_t          Count;
    bool            Full;
    pthread_mutex_t WriteLock;
    pthread_mutex_t ReadLock;
    pthread_cond_t  NotEmpty;
    pthread_cond_t  NotFull;
} circular_buffer;

sdb_errno InitCircularBuffer(circular_buffer *Cb, size_t Size);
bool      IsFull(circular_buffer *Cb);
bool      IsEmpty(circular_buffer *Cb);
ssize_t   InsertToBuffer(circular_buffer *Cb, void *Data, size_t Size);
ssize_t   ReadFromBuffer(circular_buffer *Cb, void *Dest, size_t Size);
void      FreeCircularBuffer(circular_buffer *Cb);

#endif // CIRCULARBUFFER_H
