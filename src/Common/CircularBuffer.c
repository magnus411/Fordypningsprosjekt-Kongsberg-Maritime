#include "src/Common/Thread.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <src/Sdb.h>
SDB_LOG_REGISTER(CircularBuffer);

#include <src/Common/CircularBuffer.h>
#include <src/Common/Metrics.h>

bool
CbIsFull(circular_buffer *Cb)
{
    return Cb->Full;
}

bool
CbIsEmpty(circular_buffer *Cb)
{
    return (!Cb->Full && (Cb->Head == Cb->Tail));
}


sdb_errno
CbInit(circular_buffer *Cb, size_t Size, sdb_arena *Arena)
{
    if(Size == 0) {
        SdbLogError("Error: Buffer size must be greater than zero.");
        return -EINVAL;
    }

    if(Arena == NULL) {
        Cb->Data = malloc(Size);
    } else {
        Cb->Data = SdbPushArray(Arena, u8, Size);
    }

    if(Cb->Data == NULL) {
        SdbLogError("Error: Failed to allocate memory for buffer.");
        return -ENOMEM;
    }

    Cb->DataSize = Size;
    Cb->Head     = 0;
    Cb->Tail     = 0;
    Cb->Count    = 0;
    Cb->Full     = false;

    SdbMutexInit(&Cb->WriteLock);
    SdbMutexInit(&Cb->ReadLock);
    SdbCondInit(&Cb->NotEmpty);
    SdbCondInit(&Cb->NotFull);
    SdbLogDebug("Circular buffer initialized. Size: %zu, Buffer address: %p", Size, Cb->Data);

    return 0;
}

ssize_t
CbInsert(circular_buffer *Cb, void *Data, size_t Size)
{
    pthread_mutex_lock(&Cb->WriteLock);

    while(CbIsFull(Cb)) {
        SdbLogDebug("Buffer is full. Waiting for read operation.");
        pthread_cond_wait(&Cb->NotFull, &Cb->WriteLock);
    }

    size_t FirstChunk = SdbMin(Size, Cb->DataSize - Cb->Head);
    memcpy(Cb->Data + Cb->Head, Data, FirstChunk);

    size_t SecondChunk = Size - FirstChunk;
    if(SecondChunk > 0) {
        memcpy(Cb->Data, Data + FirstChunk, SecondChunk);
    }

    Cb->Head = (Cb->Head + Size) % Cb->DataSize;
    Cb->Count += Size;
    Cb->Full = (Cb->Count == Cb->DataSize);

    MetricAddSample(&BufferWriteThroughput, Size);

    int percentageFilled = (Cb->Count * 100) / Cb->DataSize;
    MetricAddSample(&OccupancyMetric, percentageFilled);

    pthread_cond_signal(&Cb->NotEmpty);
    pthread_mutex_unlock(&Cb->WriteLock);

    return Size;
}


ssize_t
CbRead(circular_buffer *Cb, void *Dest, size_t Size)
{
    pthread_mutex_lock(&Cb->ReadLock);

    while(Cb->Count < Size) {
        pthread_cond_wait(&Cb->NotEmpty, &Cb->ReadLock);
    }

    size_t FirstChunk = SdbMin(Size, Cb->DataSize - Cb->Tail);
    memcpy(Dest, Cb->Data + Cb->Tail, FirstChunk);

    size_t SecondChunk = Size - FirstChunk;
    if(SecondChunk > 0) {
        memcpy(Dest + FirstChunk, Cb->Data, SecondChunk);
    }

    Cb->Tail = (Cb->Tail + Size) % Cb->DataSize;
    Cb->Count -= Size;
    Cb->Full = false;

    MetricAddSample(&BufferReadThroughput, Size);


    int percentageFilled = (Cb->Count * 100) / Cb->DataSize;
    MetricAddSample(&OccupancyMetric, percentageFilled);

    pthread_cond_signal(&Cb->NotFull);
    pthread_mutex_unlock(&Cb->ReadLock);

    return Size;
}

void
CbFree(circular_buffer *Cb)
{
    // NOTE(ingar): Data must be freed by user since we don't know if the memory was allocated with
    // an arena or not
    SdbMutexDeinit(&Cb->WriteLock);
    SdbMutexDeinit(&Cb->ReadLock);
    SdbCondDeinit(&Cb->NotEmpty);
    SdbCondDeinit(&Cb->NotFull);
}
