#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

#define SDB_LOG_LEVEL 4

#include "SdbExtern.h"
#include "CircularBuffer.h"

SDB_LOG_REGISTER(CircularBuffer);

sdb_errno
InitCircularBuffer(circular_buffer *Cb, size_t Size)
{
    if(Size == 0)
    {
        SdbLogError("Error: Buffer size must be greater than zero.\n");
        return -1;
    }

    Cb->Data = malloc(Size);
    if(Cb->Data == NULL)
    {
        SdbLogError("Error: Failed to allocate memory for buffer.\n");
        return -ENOMEM;
    }

    Cb->Size  = Size;
    Cb->Head  = 0;
    Cb->Tail  = 0;
    Cb->Count = 0;
    Cb->Full  = false;

    pthread_mutex_init(&Cb->WriteLock, NULL);
    pthread_mutex_init(&Cb->ReadLock, NULL);
    pthread_cond_init(&Cb->NotEmpty, NULL);
    pthread_cond_init(&Cb->NotFull, NULL);

    SdbLogDebug("Circular buffer initialized. Size: %zu, Buffer address: %p\n", Size, Cb->Data);
}

int
IsFull(circular_buffer *Cb)
{
    return Cb->Full;
}

int
IsEmpty(circular_buffer *Cb)
{
    return (!Cb->Full && (Cb->Head == Cb->Tail));
}

ssize_t
InsertToBuffer(circular_buffer *Cb, void *Data, size_t Size)
{
    pthread_mutex_lock(&Cb->WriteLock);

    while(IsFull(Cb))
    {
        SdbLogDebug("Buffer is full. Waiting for read operation.");
        pthread_cond_wait(&Cb->NotFull, &Cb->WriteLock);
    }

    /*
     size_t FreeBytes = Cb->Size - Cb->Count;
    if(Size > FreeBytes)
    {
        Size = FreeBytes;
    }*/

    size_t FirstChunk = SdbMin(Size, Cb->Size - Cb->Head);
    memcpy(Cb->Data + Cb->Head, Data, FirstChunk);

    size_t SecondChunk = Size - FirstChunk;
    if(SecondChunk > 0)
    {
        memcpy(Cb->Data, Data + FirstChunk, SecondChunk);
    }

    Cb->Head = (Cb->Head + Size) % Cb->Size;
    Cb->Count += Size;
    Cb->Full = (Cb->Count == Cb->Size);

    pthread_cond_signal(&Cb->NotEmpty);
    pthread_mutex_unlock(&Cb->WriteLock);

    return Size;
}

ssize_t
ReadFromBuffer(circular_buffer *Cb, void *Dest, size_t Size)
{
    pthread_mutex_lock(&Cb->ReadLock);

    while(IsEmpty(Cb))
    {
        pthread_cond_wait(&Cb->NotEmpty, &Cb->ReadLock);
    }

    size_t AvailableBytes = Cb->Count;
    if(Size > AvailableBytes)
    {
        SdbLogError("Error: Requested size is greater than available bytes in buffer.\n");
        return -1;
    }

    size_t FirstChunk = SdbMin(Size, Cb->Size - Cb->Tail);
    memcpy(Dest, Cb->Data + Cb->Tail, FirstChunk);

    size_t SecondChunk = Size - FirstChunk;
    if(SecondChunk > 0)
    {
        memcpy(Dest + FirstChunk, Cb->Data, SecondChunk);
    }

    Cb->Tail = (Cb->Tail + Size) % Cb->Size;
    Cb->Count -= Size;
    Cb->Full = false;

    pthread_cond_signal(&Cb->NotFull);
    pthread_mutex_unlock(&Cb->ReadLock);

    return Size;
}

void
FreeCircularBuffer(circular_buffer *Cb)
{
    free(Cb->Data);
    pthread_mutex_destroy(&Cb->WriteLock);
    pthread_mutex_destroy(&Cb->ReadLock);
    pthread_cond_destroy(&Cb->NotEmpty);
    pthread_cond_destroy(&Cb->NotFull);
}
