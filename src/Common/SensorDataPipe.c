#include <stdio.h>
#include <sys/eventfd.h>
#include <sys/poll.h>

#include <src/Sdb.h>
SDB_LOG_REGISTER(SensorDataPipe);

#include <src/Common/CircularBuffer.h>
#include <src/Common/SensorDataPipe.h>
#include <src/Common/Thread.h>
#include <src/Common/Time.h>

sensor_data_pipe *
SdpCreate(u64 BufCount, u64 BufSize, sdb_arena *Arena)
{
    sdb_arena TempArena;
    bool      UsingArena = Arena != NULL;
    if(!UsingArena) {
        u64 PipeSize = sizeof(sensor_data_pipe) + BufCount * sizeof(sdb_arena *)
                     + BufCount * sizeof(sdb_arena) + BufCount * BufSize;
        u8 *Mem = calloc(1, PipeSize);
        SdbArenaInit(&TempArena, Mem, PipeSize);
        Arena = &TempArena;
    }

    u64               ArenaF5 = SdbArenaGetPos(Arena);
    sensor_data_pipe *Pipe;
    Pipe          = SdbPushStruct(Arena, sensor_data_pipe);
    Pipe->Buffers = SdbPushArray(Arena, sdb_arena *, BufCount);
    for(u64 b = 0; b < BufCount; ++b) {
        sdb_arena *Buffer = SdbArenaBootstrap(Arena, NULL, BufSize);
        Pipe->Buffers[b]  = Buffer;
    }

    Pipe->ReadEventFd  = eventfd(0, 0 /*EFD_NONBLOCK*/);
    Pipe->WriteEventFd = eventfd(0, 0 /*EFD_NONBLOCK*/);
    // TODO(ingar): Is it correct to use non-block?

    if(Pipe->ReadEventFd == -1 || Pipe->WriteEventFd == -1) {
        SdbLogError("Failed to create event fd");
        if(UsingArena) {
            SdbArenaSeek(Arena, ArenaF5);
        } else {
            free(Arena->Mem);
        }
        return NULL;
    }

    atomic_init(&Pipe->WriteBufIdx, 0);
    atomic_init(&Pipe->ReadBufIdx, 0);
    atomic_init(&Pipe->FullBuffersCount, 0);

    Pipe->BufCount = BufCount;
    u64 InitVal    = BufCount - 1;
    write(Pipe->WriteEventFd, &InitVal, sizeof(InitVal));

    return Pipe;
}

void
SdpDestroy(sensor_data_pipe *Pipe, bool AllocatedWithArena)
{
    SdbLogDebug("Destroying sensor data pipe");
    close(Pipe->ReadEventFd);
    close(Pipe->WriteEventFd);
    if(!AllocatedWithArena) {
        free(Pipe);
    }
}

sdb_arena *
SdPipeGetWriteBuffer(sensor_data_pipe *Pipe, sdb_timediff TimeoutMs)
{
    struct pollfd Pfd = { .fd = Pipe->WriteEventFd, .events = POLLIN };

    int Result = poll(&Pfd, 1, TimeoutMs);
    if(Result == -1) {
        if(errno == EINTR) {
            return NULL;
        }
        SdbLogError("Poll failed on WriteEventFd: %s", strerror(errno));
        return NULL;
    }
    if(Result == 0) {
        // Timeout occurred
        return NULL;
    }

    u32 CurWriteBuf  = atomic_load(&Pipe->WriteBufIdx);
    u32 NextWriteBuf = (CurWriteBuf + 1) % Pipe->BufCount;

    u64 Val;
    if(read(Pipe->WriteEventFd, &Val, sizeof(Val)) == -1) {
        SdbLogError("Failed to read from WriteEventFd: %s", strerror(errno));
        return NULL;
    }

    atomic_store(&Pipe->WriteBufIdx, NextWriteBuf);
    atomic_fetch_add(&Pipe->FullBuffersCount, 1);

    Val = 1;
    if(write(Pipe->ReadEventFd, &Val, sizeof(Val)) == -1) {
        SdbLogError("Failed to write to ReadEventFd: %s", strerror(errno));
        return NULL;
    }

    sdb_arena *Buf = Pipe->Buffers[NextWriteBuf];
    SdbArenaClear(Buf);
    return Buf;
}

sdb_arena *
SdPipeGetReadBuffer(sensor_data_pipe *Pipe, sdb_timediff TimeoutMs)
{
    struct pollfd Pfd = { .fd = Pipe->ReadEventFd, .events = POLLIN };

    int Result = poll(&Pfd, 1, TimeoutMs);
    if(Result == -1) {
        if(errno == EINTR) {
            return NULL;
        }
        SdbLogError("Poll failed on ReadEventFd: %s", strerror(errno));
        return NULL;
    }
    if(Result == 0) {
        // Timeout occurred
        return NULL;
    }

    u64 Val;
    if(read(Pipe->ReadEventFd, &Val, sizeof(Val)) == -1) {
        SdbLogError("Failed to read from ReadEventFd: %s", strerror(errno));
        return NULL;
    }

    u32        CurReadBuffer = atomic_load(&Pipe->ReadBufIdx);
    sdb_arena *Buf           = Pipe->Buffers[CurReadBuffer];
    atomic_store(&Pipe->ReadBufIdx, (CurReadBuffer + 1) % Pipe->BufCount);
    atomic_fetch_sub(&Pipe->FullBuffersCount, 1);

    Val = 1;
    if(write(Pipe->WriteEventFd, &Val, sizeof(Val)) == -1) {
        SdbLogError("Failed to write to WriteEventFd: %s", strerror(errno));
        return NULL;
    }
    return Buf;
}

void
SdPipeFlush(sensor_data_pipe *Pipe, sdb_timediff TimeoutMs)
{
    sdb_arena *CurrentBuf = Pipe->Buffers[atomic_load(&Pipe->WriteBufIdx)];
    if(SdbArenaGetPos(CurrentBuf) > 0) {
        struct pollfd Pfd = { .fd = Pipe->WriteEventFd, .events = POLLIN };

        int Result = poll(&Pfd, 1, TimeoutMs);
        if(Result == -1) {
            if(errno == EINTR) {
                SdbLogWarning("Flush interrupted while waiting for buffer");
                return;
            }
            SdbLogError("Poll failed on WriteEventFd during flush: %s", strerror(errno));
            return;
        }
        if(Result == 0) {
            SdbLogWarning("Flush timed out while waiting for buffer");
            return;
        }

        u32 CurWriteBuf  = atomic_load(&Pipe->WriteBufIdx);
        u32 NextWriteBuf = (CurWriteBuf + 1) % Pipe->BufCount;

        u64 Val;
        if(read(Pipe->WriteEventFd, &Val, sizeof(Val)) == -1) {
            SdbLogError("Failed to read from WriteEventFd during flush: %s", strerror(errno));
            return;
        }

        atomic_store(&Pipe->WriteBufIdx, NextWriteBuf);
        atomic_fetch_add(&Pipe->FullBuffersCount, 1);

        Val = 1;
        if(write(Pipe->ReadEventFd, &Val, sizeof(Val)) == -1) {
            SdbLogError("Failed to write to ReadEventFd during flush: %s", strerror(errno));
            return;
        }
    }
}
