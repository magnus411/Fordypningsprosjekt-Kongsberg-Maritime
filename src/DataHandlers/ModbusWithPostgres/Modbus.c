#include <signal.h>

#include <src/Sdb.h>
SDB_LOG_DECLARE(Modbus);
SDB_THREAD_ARENAS_EXTERN(Modbus);

#include <src/CommProtocols/Modbus.h>
#include <src/Common/SensorDataPipe.h>
#include <src/Common/Socket.h>
#include <src/Common/Thread.h>
#include <src/DataHandlers/ModbusWithPostgres/ModbusWithPostgres.h>
#include <src/DevUtils/TestConstants.h>

extern volatile sig_atomic_t GlobalShutdown;

sdb_errno
MbPipeThroughputTest(void *Arg)
{
    sdb_errno Ret = 0;
    mbpg_ctx *Ctx = Arg;

    sdb_arena MbArena;
    u64       MbASize = Ctx->ModbusMemSize + MB_SCRATCH_COUNT * Ctx->ModbusScratchSize;
    u8       *MbAMem  = malloc(MbASize);
    SdbArenaInit(&MbArena, MbAMem, MbASize);

    MbThreadArenasInit();
    SdbThreadArenasInitExtern(Modbus);
    for(u64 s = 0; s < MB_SCRATCH_COUNT; ++s) {
        sdb_arena *Scratch = SdbArenaBootstrap(&MbArena, NULL, Ctx->ModbusScratchSize);
        SdbThreadArenasAdd(Scratch);
    }


    sensor_data_pipe *Pipe     = Ctx->SdPipe;
    sdb_arena        *CurBuf   = Pipe->Buffers[atomic_load(&Pipe->WriteBufIdx)];
    sdb_file_data    *TestData = SdbLoadFileIntoMemory("./data/testdata/TestData.sdb", NULL);

    SdbLogInfo("Modbus thread successfully initialized. Waiting for other threads at barrier");
    SdbBarrierWait(&Ctx->Barrier);
    SdbLogInfo("Exited barrier. Starting main loop");

    i64 RemainingData = TestData->Size;
    while(RemainingData > 0) {
        i64 Size = (RemainingData < Pipe->BufferMaxFill) ? RemainingData : Pipe->BufferMaxFill;
        u8 *Ptr  = SdbArenaPush(CurBuf, Size);
        SdbMemcpy(Ptr, TestData->Data + (TestData->Size - RemainingData), Size);
        CurBuf = SdPipeGetWriteBuffer(Pipe, SDB_TIME_MS(100));
        RemainingData -= Pipe->BufferMaxFill;
    }

    SdPipeFlush(Pipe, SDB_TIME_MS(100));
    SdbLogDebug("Modbus main loop stopped with %s", (Ret == 0) ? "success" : "error");

    free(MbAMem);
    free(TestData);
    return Ret;
}


sdb_errno
MbRun(void *Arg)
{
    sdb_errno Ret = 0;
    mbpg_ctx *Ctx = Arg;

    sdb_arena MbArena;
    u64       MbASize = Ctx->ModbusMemSize + MB_SCRATCH_COUNT * Ctx->ModbusScratchSize;
    u8       *MbAMem  = malloc(MbASize);
    SdbArenaInit(&MbArena, MbAMem, MbASize);

    MbThreadArenasInit();
    SdbThreadArenasInitExtern(Modbus);
    for(u64 s = 0; s < MB_SCRATCH_COUNT; ++s) {
        sdb_arena *Scratch = SdbArenaBootstrap(&MbArena, NULL, Ctx->ModbusScratchSize);
        SdbThreadArenasAdd(Scratch);
    }

    sensor_data_pipe *Pipe   = Ctx->SdPipe;
    sdb_arena        *CurBuf = Pipe->Buffers[atomic_load(&Pipe->WriteBufIdx)];

    SdbBarrierWait(&Ctx->ServerBarrier);

    modbus_ctx *MbCtx = MbPrepareCtx(&MbArena);
    if(MbCtx == NULL) {
        SdbLogError("Failed to prepare modbus context. Exiting");
        free(MbAMem);
        return -1;
    }

    mb_conn Conn = MbCtx->Conns[0]; // NOTE(ingar): Simplified to only use one


    SdbLogInfo("Modbus thread waiting for other threads at barrier");
    SdbBarrierWait(&Ctx->Barrier);
    SdbLogInfo("Exited barrier. Starting main loop");

    int LogCounter = 0;
    while(!GlobalShutdown) {
        if(++LogCounter % 1000 == 0) {
            SdbLogDebug("Modbus loop is still running");
        }
        SdbAssert((SdbArenaGetPos(CurBuf) <= Pipe->BufferMaxFill),
                  "Pipe buffer overflow in buffer %u", atomic_load(&Pipe->WriteBufIdx));

        if(SdbArenaGetPos(CurBuf) == Pipe->BufferMaxFill) {
            CurBuf = SdPipeGetWriteBuffer(Pipe, SDB_TIME_MS(100));
        }

        u8  Frame[MODBUS_TCP_FRAME_MAX_SIZE];
        int RecvResult = SocketRecvWithTimeout(Conn.SockFd, Frame, sizeof(Frame), SDB_TIME_MS(500));

        switch(RecvResult) {
            case -2: // Timeout
                continue;
            case -1: // Error
                Ret = -1;
                goto exit;
                break;
            case 0: // Server disconnected
                Ret = 0;
                SdbSleep(SDB_TIME_MS(500)); // Wait for GlobalShutdown to be set
                break;
            default: // Data received
                {
                    u16       UnitId, DataLength;
                    const u8 *Data = MbParseTcpFrame(Frame, &UnitId, &DataLength);

                    SdbAssert(DataLength == Pipe->PacketSize,
                              "Modbus packet size did not match expected, was %u expected %zd",
                              DataLength, Pipe->PacketSize);

                    u8 *Ptr = SdbArenaPush(CurBuf, DataLength);
                    SdbMemcpy(Ptr, Data, DataLength);
                }
                break;
        }
    }

exit:
    SdPipeFlush(Pipe, SDB_TIME_MS(100));
    SdbLogDebug("Modbus main loop stopped with %s", (Ret == 0) ? "success" : "error");

    for(u64 i = 0; i < MbCtx->ConnCount; ++i) {
        int SockFd = MbCtx->Conns[i].SockFd;
        SdbLogDebug("Closing connection %d", SockFd);
        close(SockFd);
    }

    free(MbAMem);

    return Ret;
}
