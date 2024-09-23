#include "CircularBuffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "SdbExtern.h"

void
InitCircularBuffer(CircularBuffer *Cb, size_t Size)
{
    if(Size == 0)
    {
        fprintf(stderr, "Error: Buffer size must be greater than zero.\n");
        exit(EXIT_FAILURE);
    }

    Cb->Buffer = malloc(Size);
    if(Cb->Buffer == NULL)
    {
        fprintf(stderr, "Error: Failed to allocate memory for buffer.\n");
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

    printf("Circular buffer initialized. Size: %zu, Buffer address: %p\n", Size, Cb->Buffer);
}

int
IsFull(CircularBuffer *Cb)
{
    return Cb->Full;
}

int
IsEmpty(CircularBuffer *Cb)
{
    return (!Cb->Full && (Cb->Head == Cb->Tail));
}

size_t
InsertToBuffer(CircularBuffer *Cb, void *Data, size_t Size)
{
    pthread_mutex_lock(&Cb->WriteLock);

    if(Cb->Size == 0 || Cb->Buffer == NULL)
    {
        fprintf(stderr,
                "Error: Circular buffer size is zero or buffer is NULL during insertion.\n");
        pthread_mutex_unlock(&Cb->WriteLock);
        return 0;
    }

    while(IsFull(Cb))
    {
        fprintf(stderr, "Buffer is full. Waiting for read operation.\n");
        pthread_cond_wait(&Cb->NotFull, &Cb->WriteLock);
    }

    if(Cb->Head >= Cb->Size)
    {
        fprintf(stderr, "Error: Buffer head is out of bounds. Head: %zu, Size: %zu\n", Cb->Head,
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
    memcpy((uint8_t *)Cb->Buffer + Cb->Head, Data, FirstChunk);

    size_t SecondChunk = Size - FirstChunk;
    if(SecondChunk > 0)
    {
        memcpy(Cb->Buffer, (uint8_t *)Data + FirstChunk, SecondChunk);
    }

    Cb->Head = (Cb->Head + Size) % Cb->Size;
    Cb->Count += Size;
    Cb->Full = (Cb->Count == Cb->Size);

    pthread_cond_signal(&Cb->NotEmpty);
    pthread_mutex_unlock(&Cb->WriteLock);

    return Size;
}

size_t
ReadFromBuffer(CircularBuffer *Cb, void *Dest, size_t Size)
{
    pthread_mutex_lock(&Cb->ReadLock);

    if(Cb->Size == 0 || Cb->Buffer == NULL)
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
    memcpy(Dest, (uint8_t *)Cb->Buffer + Cb->Tail, FirstChunk);

    size_t SecondChunk = Size - FirstChunk;
    if(SecondChunk > 0)
    {
        memcpy((uint8_t *)Dest + FirstChunk, Cb->Buffer, SecondChunk);
    }

    Cb->Tail = (Cb->Tail + Size) % Cb->Size;
    Cb->Count -= Size;
    Cb->Full = 0;

    pthread_cond_signal(&Cb->NotFull);
    pthread_mutex_unlock(&Cb->ReadLock);

    return Size;
}

void
FreeCircularBuffer(CircularBuffer *Cb)
{
    free(Cb->Buffer);
    pthread_mutex_destroy(&Cb->WriteLock);
    pthread_mutex_destroy(&Cb->ReadLock);
    pthread_cond_destroy(&Cb->NotEmpty);
    pthread_cond_destroy(&Cb->NotFull);
}
