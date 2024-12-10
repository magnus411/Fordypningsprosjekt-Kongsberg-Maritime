#include <src/Sdb.h>

SDB_LOG_DECLARE(Modbus);
SDB_THREAD_ARENAS_EXTERN(Modbus);

#include <src/CommProtocols/CommProtocols.h>
#include <src/CommProtocols/Modbus.h>
#include <src/Common/SensorDataPipe.h>
#include <src/DevUtils/TestConstants.h>

sdb_errno
MbInit(comm_protocol_api *Mb)
{
    MbThreadArenasInit();
    SdbThreadArenasInitExtern(Modbus);
    sdb_arena *Scratch1 = SdbArenaBootstrap(&Mb->Arena, NULL, SdbKibiByte(512));
    sdb_arena *Scratch2 = SdbArenaBootstrap(&Mb->Arena, NULL, SdbKibiByte(512));
    SdbThreadArenasAdd(Scratch1);
    SdbThreadArenasAdd(Scratch2);

    return 0;
}

sdb_errno
MbRun(comm_protocol_api *Mb)
{
    sdb_errno Ret = 0;

    sdb_thread_control *ModuleControl = Mb->ModuleControl;
    sdb_arena          *MbArena       = &Mb->Arena;

    sensor_data_pipe *Pipe = Mb->SdPipes[0]; // TODO(ingar): Simplified to only use one pipe for now
    sdb_arena        *CurBuf = Pipe->Buffers[atomic_load(&Pipe->WriteBufIdx)];

    sdb_file_data *TestData = SdbLoadFileIntoMemory("./data/TestData.sdb", NULL);

    u64 RemainingData = TestData->Size / Pipe->BufferMaxFill;
    while(RemainingData > 0) {
        SdbMemcpy(CurBuf->Mem, TestData->Data + (TestData->Size - RemainingData),
                  Pipe->BufferMaxFill);
        SdPipeGetWriteBuffer(Pipe);
    }

    SdPipeFlush(Pipe);
    SdbLogDebug("Modbus main loop stopped with %s", (Ret == 0) ? "success" : "error");

    return Ret;
}

sdb_errno
MbFinalize(comm_protocol_api *Mb)
{
    sdb_errno   Ret   = 0;
    modbus_ctx *MbCtx = MB_CTX(Mb);

    for(u64 i = 0; i < MbCtx->ConnCount; ++i) {
        int SockFd = MbCtx->Conns[i].SockFd;
        SdbLogDebug("Closing connection %d", SockFd);
        close(SockFd);
    }

    return Ret;
}
