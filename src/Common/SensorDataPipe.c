#include <stdio.h>
#include <sys/eventfd.h>

#include <src/Sdb.h>
SDB_LOG_REGISTER(SensorDataPipe);

#include <src/Common/CircularBuffer.h>
#include <src/Common/SensorDataPipe.h>
#include <src/Common/Thread.h>
#include <src/Common/Time.h>

sensor_data_pipe **
SdPipesInit(u64 PipeCount, u64 BufCount, size_t BufSize, sdb_arena *Arena)
{
    // TODO(ingar): The initialization is done in this manner to ensure that the pipes are
    // contiguous with their buffers, and the buffers with their memory
    u64 ArenaF5 = SdbArenaGetPos(Arena);

    sensor_data_pipe **Pipes = SdbPushArray(Arena, sensor_data_pipe *, PipeCount);
    for(u64 p = 0; p < PipeCount; ++p) {

        sensor_data_pipe *Pipe = SdbPushStruct(Arena, sensor_data_pipe);
        Pipe->ReadEventFd      = eventfd(0, 0 /*EFD_NONBLOCK*/);
        Pipe->WriteEventFd     = eventfd(0, 0 /*EFD_NONBLOCK*/);
        if(Pipe->ReadEventFd == -1 || Pipe->WriteEventFd == -1) {
            SdbLogError("Failed to create event fd");
            SdbArenaSeek(Arena, ArenaF5);
            return NULL;
        }

        Pipe->Buffers = SdbPushArray(Arena, sdb_arena *, BufCount);
        for(u64 b = 0; b < BufCount; ++b) {
            sdb_arena *Buffer = SdbArenaBootstrap(Arena, NULL, BufSize);
            Pipe->Buffers[b]  = Buffer;
        }

        atomic_init(&Pipe->WriteBufIdx, 0);
        atomic_init(&Pipe->ReadBufIdx, 0);
        atomic_init(&Pipe->FullBuffersCount, 0);

        Pipe->BufCount = BufCount;
        u64 InitVal    = BufCount - 1;
        write(Pipe->WriteEventFd, &InitVal, sizeof(InitVal));

        Pipes[p] = Pipe;
    }

    SdbLogInfo("Sensor data pipes initialized successfully");

    return Pipes;
}

sdb_arena *
SdPipeGetWriteBuffer(sensor_data_pipe *Pipe)
{
    u32 CurWriteBuf  = atomic_load(&Pipe->WriteBufIdx);
    u32 NextWriteBuf = (CurWriteBuf + 1) % Pipe->BufCount;

    // Block until a slot is available
    u64 Val;
    if(read(Pipe->WriteEventFd, &Val, sizeof(Val)) == -1) {
        if(errno == EINTR) {
            // Handle interrupt if needed
            return NULL;
        }
        SdbLogError("Failed to read from WriteEventFd");
        return NULL;
    }

    atomic_store(&Pipe->WriteBufIdx, NextWriteBuf);
    atomic_fetch_add(&Pipe->FullBuffersCount, 1);

    // Signal reader that new data is available
    Val = 1;
    if(write(Pipe->ReadEventFd, &Val, sizeof(Val)) == -1) {
        SdbLogError("Failed to write to ReadEventFd");
        return NULL;
    }

    sdb_arena *Buf = Pipe->Buffers[NextWriteBuf];
    SdbArenaClear(Buf);
    return Buf;
}

sdb_arena *
SdPipeGetReadBuffer(sensor_data_pipe *Pipe)
{
    // Block until data is available
    u64 Val;
    if(read(Pipe->ReadEventFd, &Val, sizeof(Val)) == -1) {
        if(errno == EINTR) {
            // Handle interrupt if needed
            return NULL;
        }
        SdbLogError("Failed to read from ReadEventFd");
        return NULL;
    }

    u32        CurReadBuffer = atomic_load(&Pipe->ReadBufIdx);
    sdb_arena *Buf           = Pipe->Buffers[CurReadBuffer];
    atomic_store(&Pipe->ReadBufIdx, (CurReadBuffer + 1) % Pipe->BufCount);
    atomic_fetch_sub(&Pipe->FullBuffersCount, 1);

    // Signal writer that a slot is available
    Val = 1;
    if(write(Pipe->WriteEventFd, &Val, sizeof(Val)) == -1) {
        SdbLogError("Failed to write to WriteEventFd");
        return NULL;
    }

    return Buf;
}


void
SdPipeFlush(sensor_data_pipe *Pipe)
{
    sdb_arena *CurrentBuf = Pipe->Buffers[atomic_load(&Pipe->WriteBufIdx)];
    if(SdbArenaGetPos(CurrentBuf) > 0) {
        u32 CurWriteBuf  = atomic_load(&Pipe->WriteBufIdx);
        u32 NextWriteBuf = (CurWriteBuf + 1) % Pipe->BufCount;

        // Block until a slot is available
        u64 Val;
        if(read(Pipe->WriteEventFd, &Val, sizeof(Val)) == -1) {
            if(errno == EINTR) {
                SdbLogWarning("Flush interrupted while waiting for buffer");
                return;
            }
            SdbLogError("Failed to read from WriteEventFd during flush: %s", strerror(errno));
            return;
        }

        atomic_store(&Pipe->WriteBufIdx, NextWriteBuf);
        atomic_fetch_add(&Pipe->FullBuffersCount, 1);

        // Signal reader that new data is available
        Val = 1;
        if(write(Pipe->ReadEventFd, &Val, sizeof(Val)) == -1) {
            SdbLogError("Failed to write to ReadEventFd during flush: %s", strerror(errno));
            return;
        }

        sdb_arena *NextBuf = Pipe->Buffers[NextWriteBuf];
        SdbArenaClear(NextBuf);
    }
}
