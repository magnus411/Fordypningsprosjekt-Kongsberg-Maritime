#include "CircularBuffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SdbExtern.h"

SDB_LOG_REGISTER(CircularBuffer);

void
InitCircularBuffer(circular_buffer *Cb, size_t Size)
{
    if(Size == 0)
    {
        SdbLogError("Error: Buffer size must be greater than zero.\n");
        exit(EXIT_FAILURE);
    }

    Cb->Data = malloc(Size);
    if(Cb->Data == NULL)
    {
        SdbLogError("Error: Failed to allocate memory for buffer.\n");
        exit(EXIT_FAILURE);
    }

    Cb->Size  = Size;
    Cb->Head  = 0;
    Cb->Tail  = 0;
    Cb->Count = 0;
    Cb->Full  = 0;

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

size_t
InsertToBuffer(circular_buffer *Cb, void *Data, size_t Size)
{
    pthread_mutex_lock(&Cb->WriteLock);

    if(Cb->Size == 0 || Cb->Data == NULL)
    {
        SdbLogDebug("Error: Circular buffer size is zero or buffer is NULL during insertion.");
        pthread_mutex_unlock(&Cb->WriteLock);
        return 0;
    }

    while(IsFull(Cb))
    {
        SdbLogDebug("Buffer is full. Waiting for read operation.");
        pthread_cond_wait(&Cb->NotFull, &Cb->WriteLock);
    }

    if(Cb->Head >= Cb->Size)
    {
        SdbLogError("Error: Buffer head is out of bounds. Head: %zu, Size: %zu\n", Cb->Head,
                    Cb->Size);
        pthread_mutex_unlock(&Cb->WriteLock);
        return 0;
    }

    size_t FreeBytes = Cb->Size - Cb->Count;
    if(Size > FreeBytes)
    {
        Size = FreeBytes;
    }

    size_t FirstChunk = SdbMin(Size, Cb->Size - Cb->Head);
    memcpy((uint8_t *)Cb->Data + Cb->Head, Data, FirstChunk);

    size_t SecondChunk = Size - FirstChunk;
    if(SecondChunk > 0)
    {
        memcpy(Cb->Data, (uint8_t *)Data + FirstChunk, SecondChunk);
    }

    Cb->Head = (Cb->Head + Size) % Cb->Size;
    Cb->Count += Size;
    Cb->Full = (Cb->Count == Cb->Size);

    pthread_cond_signal(&Cb->NotEmpty);
    pthread_mutex_unlock(&Cb->WriteLock);

    return Size;
}

size_t
ReadFromBuffer(circular_buffer *Cb, void *Dest, size_t Size)
{
    pthread_mutex_lock(&Cb->ReadLock);

    if(Cb->Size == 0 || Cb->Data == NULL)
    {
        fprintf(stderr, "Error: Circular buffer size is zero or buffer is NULL during read.\n");
        pthread_mutex_unlock(&Cb->ReadLock);
        return 0;
    }

    while(IsEmpty(Cb))
    {
        pthread_cond_wait(&Cb->NotEmpty, &Cb->ReadLock);
    }

    if(Cb->Tail >= Cb->Size)
    {
        fprintf(stderr, "Error: Buffer tail is out of bounds. Tail: %zu, Size: %zu\n", Cb->Tail,
                Cb->Size);
        pthread_mutex_unlock(&Cb->ReadLock);
        return 0;
    }

    size_t AvailableBytes = Cb->Count;
    if(Size > AvailableBytes)
    {
        Size = AvailableBytes;
    }

    size_t FirstChunk = SdbMin(Size, Cb->Size - Cb->Tail);
    memcpy(Dest, (uint8_t *)Cb->Data + Cb->Tail, FirstChunk);

    size_t SecondChunk = Size - FirstChunk;
    if(SecondChunk > 0)
    {
        memcpy((uint8_t *)Dest + FirstChunk, Cb->Data, SecondChunk);
    }

    Cb->Tail = (Cb->Tail + Size) % Cb->Size;
    Cb->Count -= Size;
    Cb->Full = 0;

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
