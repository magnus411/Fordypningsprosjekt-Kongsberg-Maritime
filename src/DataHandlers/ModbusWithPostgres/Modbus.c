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
#include <src/Signals.h>


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
        CurBuf = SdPipeGetWriteBuffer(Pipe);
        RemainingData -= Pipe->BufferMaxFill;
    }

    SdPipeFlush(Pipe);
    SdbLogDebug("Modbus main loop stopped with %s", (Ret == 0) ? "success" : "error");

    free(MbAMem);
    free(TestData);
    return Ret;
}


sdb_errno
MbRun(void *Arg)
{
    sdb_errno Ret       = 0;
    mbpg_ctx *Ctx       = Arg;
    bool      first_run = true;

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

    // Only wait at barrier first time
    if(first_run) {
        SdbLogInfo("Modbus thread successfully initialized. Waiting for other threads at barrier");
        SdbBarrierWait(&Ctx->Barrier);
        SdbLogInfo("Exited barrier. Starting main loop");
        first_run = false;
    }

    while(!SdbShouldShutdown()) {
        // Create new context and connection for each attempt
        modbus_ctx *MbCtx = MbPrepareCtx(&MbArena);
        if(!MbCtx) {
            SdbLogError("Failed to prepare Modbus context");
            usleep(1000000); // Wait 1 second before retry
            continue;
        }

        mb_conn Conn       = MbCtx->Conns[0];
        int     LogCounter = 0;

        // Connection loop

        static int counter = 0;
        while(!SdbShouldShutdown()) {


            SdbAssert((SdbArenaGetPos(CurBuf) <= Pipe->BufferMaxFill),
                      "Pipe buffer overflow in buffer %u", atomic_load(&Pipe->WriteBufIdx));

            if(SdbArenaGetPos(CurBuf) == Pipe->BufferMaxFill) {
                CurBuf = SdPipeGetWriteBuffer(Pipe);
            }

            u8  Frame[MODBUS_TCP_FRAME_MAX_SIZE];
            int RecvResult
                = SocketRecvWithTimeout(Conn.SockFd, Frame, sizeof(Frame), SDB_TIME_MS(500));

            switch(RecvResult) {
                case -2: // Timeout
                    continue;
                case -1: // Error
                case 0:  // Server disconnected
                    SdbLogInfo("Connection lost or error, will retry");
                    close(Conn.SockFd);
                    goto reconnect;
                default: // Data received
                    {
                        u8      Frame[MODBUS_TCP_FRAME_MAX_SIZE] = { 0 };
                        ssize_t HeaderResult                     = SocketRecvWithTimeout(
                            Conn.SockFd, Frame, MODBUS_TCP_HEADER_LEN, SDB_TIME_MS(500));
                        if(HeaderResult <= 0) {
                            if(HeaderResult == -2) {
                                continue; // Timeout
                            }
                            goto reconnect;
                        }
                        if(HeaderResult != MODBUS_TCP_HEADER_LEN) {
                            SdbLogError("Incomplete header received: %zd bytes", HeaderResult);
                            goto reconnect;
                        }

                        u16 Length = (Frame[4] << 8) | Frame[5];
                        if(Length > MODBUS_TCP_FRAME_MAX_SIZE - MODBUS_TCP_HEADER_LEN) {
                            SdbLogError("Invalid frame length in header: %u", Length);
                            goto reconnect;
                        }

                        ssize_t DataResult = SocketRecvWithTimeout(
                            Conn.SockFd, Frame + MODBUS_TCP_HEADER_LEN, Length, SDB_TIME_MS(500));


                        if(DataResult != Length) {
                            SdbLogError("Incomplete data received: %zd of %u bytes", DataResult,
                                        Length);
                            goto reconnect;
                        }
                        u16       UnitId, DataLength;
                        const u8 *Data = MbParseTcpFrame(Frame, &UnitId, &DataLength);
                        if(!Data) {
                            SdbLogError("Failed to parse frame");
                            goto reconnect;
                        }

                        if(DataLength != Pipe->PacketSize) {
                            SdbLogError("Size mismatch: got %u, expected %zu", DataLength,
                                        Pipe->PacketSize);
                            goto reconnect;
                        }


                        u8 *Ptr = SdbArenaPush(CurBuf, DataLength);
                        SdbMemcpy(Ptr, Data, DataLength);
                        counter++;
                        if(counter % 10000 == 0) {
                            SdbLogInfo("Received %d packets", counter);
                        }
                    }
                    break;
            }
        }

reconnect:
        SdPipeFlush(Pipe);
        // Clean up current connection before retry
        for(u64 i = 0; i < MbCtx->ConnCount; ++i) {
            if(MbCtx->Conns[i].SockFd != -1) {
                close(MbCtx->Conns[i].SockFd);
                MbCtx->Conns[i].SockFd = -1;
            }
        }

        if(!SdbShouldShutdown()) {
            SdbLogInfo("Will attempt reconnection in 1 second");
            usleep(1000000); // Wait 1 second before retry
        }
    }

    free(MbAMem);
    return Ret;
}
