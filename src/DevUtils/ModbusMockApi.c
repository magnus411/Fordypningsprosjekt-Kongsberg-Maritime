#include <src/Sdb.h>

SDB_LOG_REGISTER(ModbusMock);

#include <src/CommProtocols/CommProtocols.h>
#include <src/CommProtocols/Modbus.h>
#include <src/Common/SensorDataPipe.h>


sdb_errno
MbMockInit(comm_protocol_api *Mb)
{
    return 0;
}

sdb_errno
MbMockRun(comm_protocol_api *Mb)
{
    sdb_errno         Ret      = 0;
    sensor_data_pipe *Pipe     = Mb->SdPipes[0];
    sdb_arena        *CurBuf   = Pipe->Buffers[atomic_load(&Pipe->WriteBufIdx)];
    sdb_file_data    *TestData = SdbLoadFileIntoMemory("./data/TestData.sdb", NULL);

    i64 RemainingData = TestData->Size;
    while(RemainingData > 0) {
        i64 Size = (RemainingData < Pipe->BufferMaxFill) ? RemainingData : Pipe->BufferMaxFill;
        u8 *Ptr  = SdbArenaPush(CurBuf, Size);
        SdbMemcpy(Ptr, TestData->Data + (TestData->Size - RemainingData), Size);
        CurBuf = SdPipeGetWriteBuffer(Pipe);
        RemainingData -= Pipe->BufferMaxFill;
    }

    SdPipeFlush(Pipe);
    SdbLogDebug("Modbus mock main loop stopped with %s", (Ret == 0) ? "success" : "error");
    free(TestData);

    return Ret;
}

sdb_errno
MbMockFinalize(comm_protocol_api *Mb)
{
    return 0;
}

sdb_errno
MbMockApiInit(Comm_Protocol_Type Type, sdb_thread_control *ModuleControl, u64 SensorCount,
              sensor_data_pipe **SdPipes, sdb_arena *Arena, u64 ArenaSize, i64 CommTId,
              comm_protocol_api *CpApi)
{
    SdbMemset(CpApi, 0, sizeof(*CpApi));
    CpApi->ModuleControl = ModuleControl;
    CpApi->SensorCount   = SensorCount;
    CpApi->SdPipes       = SdPipes;
    SdbArenaBootstrap(Arena, &CpApi->Arena, ArenaSize);
    CpApi->Init     = MbMockInit;
    CpApi->Run      = MbMockRun;
    CpApi->Finalize = MbMockFinalize;

    return 0;
}
