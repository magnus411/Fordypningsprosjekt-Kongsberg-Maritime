#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <pthread.h>
#include <stdint.h>
#include <stdbool.h>

#include "SdbExtern.h"
#define MAX_DATA_LENGTH 260

typedef struct
{
    u8             *Data;
    size_t          Size;
    size_t          Head;
    size_t          Tail;
    size_t          Count;
    int             Full;
    pthread_mutex_t WriteLock;
    pthread_mutex_t ReadLock;
    pthread_cond_t  NotEmpty;
    pthread_cond_t  NotFull;
} circular_buffer;

void   InitCircularBuffer(circular_buffer *Cb, size_t Size);
int    IsFull(circular_buffer *Cb);
int    IsEmpty(circular_buffer *Cb);
size_t InsertToBuffer(circular_buffer *Cb, void *Data, size_t Size);
size_t ReadFromBuffer(circular_buffer *Cb, void *Dest, size_t Size);
void   FreeCircularBuffer(circular_buffer *Cb);

#endif // CIRCULARBUFFER_H