#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <syscall.h>
#include <src/Sdb.h>

SDB_LOG_REGISTER(CircularBuffer);

#include <src/Common/CircularBuffer.h>
#include <src/Metrics.h>
sdb_errno
CbInit(circular_buffer *Cb, size_t Size, sdb_arena *Arena)
{
    if (Size == 0) {
        SdbLogError("Error: Buffer size must be greater than zero.");
        return -EINVAL;
    }

    size_t PageSize = getpagesize();
    Size = (Size + PageSize - 1) & ~(PageSize - 1);

    Cb->Fd = memfd_create("circular_buffer", 0);
    if (Cb->Fd == -1) {
        SdbLogError("Error: Failed to create memory file descriptor");
        return -ENOMEM;
    }

    if (ftruncate(Cb->Fd, Size) == -1) {
        SdbLogError("Error: Failed to set file size");
        return -ENOMEM;
    }

    void *Addr = mmap(NULL, 2 * Size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (Addr == MAP_FAILED) {
        SdbLogError("Error: Failed to reserve address space");
        return -ENOMEM;
    }

    Cb->Data = mmap(Addr, Size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, Cb->Fd, 0);
    if (Cb->Data == MAP_FAILED) {
        SdbLogError("Error: Failed to map first buffer region");
        return -ENOMEM;
    }

    void *SecondMapping = mmap(Addr + Size, Size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_FIXED, Cb->Fd, 0);
    if (SecondMapping == MAP_FAILED) {
        SdbLogError("Error: Failed to map second buffer region");
        return -ENOMEM;
    }

    Cb->DataSize = Size;
    Cb->Head = 0;
    Cb->Tail = 0;
    Cb->Count = 0;
    Cb->Full = false;

    pthread_mutex_init(&Cb->WriteLock, NULL);
    pthread_mutex_init(&Cb->ReadLock, NULL);
    pthread_cond_init(&Cb->NotEmpty, NULL);
    pthread_cond_init(&Cb->NotFull, NULL);

    SdbLogDebug("Circular buffer initialized. Size: %zu, Buffer address: %p", Size, Cb->Data);
    return 0;
}

ssize_t
CbInsert(circular_buffer *Cb, void *Data, size_t Size)
{
    pthread_mutex_lock(&Cb->WriteLock);
    
    while (CbIsFull(Cb)) {
        SdbLogDebug("Buffer is full. Waiting for read operation.");
        pthread_cond_wait(&Cb->NotFull, &Cb->WriteLock);
    }

    memcpy(Cb->Data + Cb->Head, Data, Size);

    Cb->Head = (Cb->Head + Size) % Cb->DataSize;
    Cb->Count += Size;
    Cb->Full = (Cb->Count == Cb->DataSize);

    AddSample(&BufferWriteThroughput, Size);
    int percentageFilled = (Cb->Count * 100) / Cb->DataSize;
    AddSample(&OccupancyMetric, percentageFilled);

    pthread_cond_signal(&Cb->NotEmpty);
    pthread_mutex_unlock(&Cb->WriteLock);
    return Size;
}

ssize_t
CbRead(circular_buffer *Cb, void *Dest, size_t Size)
{
    pthread_mutex_lock(&Cb->ReadLock);
    
    while (Cb->Count < Size) {
        pthread_cond_wait(&Cb->NotEmpty, &Cb->ReadLock);
    }

    memcpy(Dest, Cb->Data + Cb->Tail, Size);

    Cb->Tail = (Cb->Tail + Size) % Cb->DataSize;
    Cb->Count -= Size;
    Cb->Full = false;

    AddSample(&BufferReadThroughput, Size);
    int percentageFilled = (Cb->Count * 100) / Cb->DataSize;
    AddSample(&OccupancyMetric, percentageFilled);

    pthread_cond_signal(&Cb->NotFull);
    pthread_mutex_unlock(&Cb->ReadLock);
    return Size;
}

void
CbFree(circular_buffer *Cb)
{
    munmap(Cb->Data, 2 * Cb->DataSize);
    close(Cb->Fd);

    pthread_mutex_destroy(&Cb->WriteLock);
    pthread_mutex_destroy(&Cb->ReadLock);
    pthread_cond_destroy(&Cb->NotEmpty);
    pthread_cond_destroy(&Cb->NotFull);
}
