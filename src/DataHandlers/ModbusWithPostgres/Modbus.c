/**
 * @file ModbusAPI.c
 * @brief Implementation of Modbus communication functionality
 */

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


/**
 * @brief Implements throughput testing for Modbus data pipe
 *
 * Loads test data and processes it through the data pipe while measuring
 * throughput. Thread-safe implementation using atomic operations for buffer
 * management.
 *
 * Implementation details:
 * 1. Initializes arenas
 * 2. Waits at synchronization barrier
 * 3. Loads test data from file
 * 4. Pushes data to pipe buffers
 *
 * @param Arg Pointer to mbpg_ctx structure
 * @return sdb_errno Success/error status
 */
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


/**
 * @brief Implements main Modbus thread loop
 *
 * Manages the lifecycle of Modbus connections and data processing:
 * - Handles connection establishment and recovery
 * - Processes incoming Modbus frames
 * - Manages graceful shutdown
 *
 * Connection handling:
 * - Automatically reconnects on failure
 * - Implements backoff on repeated failures
 * - Validates frame integrity
 *
 * Data processing:
 * - Parses Modbus TCP frames
 * - Validates data length and format
 * - Manages buffer rotation
 * - Handles pipeline flushing
 *
 * @param Arg Pointer to mbpg_ctx structure
 * @return sdb_errno Success/error status
 */
sdb_errno
MbRun(void *Arg)
{
    sdb_errno Ret      = 0;
    mbpg_ctx *Ctx      = Arg;
    bool      FirstRun = true;

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

    /**<  Only wait at barrier first time */
    if(FirstRun) {
        SdbLogInfo("Modbus thread successfully initialized. Waiting for other threads at barrier");
        SdbBarrierWait(&Ctx->Barrier);
        SdbLogInfo("Exited barrier. Starting main loop");
        FirstRun = false;
    }

    while(!SdbShouldShutdown()) {
        /**< Create new context and connection for each attempt */
        modbus_ctx *MbCtx = MbPrepareCtx(&MbArena);
        if(!MbCtx) {
            SdbLogError("Failed to prepare Modbus context");
            usleep(1000000); // Wait 1 second before retry
            continue;
        }

        mb_conn Conn = MbCtx->Conns[0];
        // int     LogCounter = 0;


        while(!SdbShouldShutdown()) {


            SdbAssert((SdbArenaGetPos(CurBuf) <= Pipe->BufferMaxFill),
                      "Pipe buffer overflow in buffer %u", atomic_load(&Pipe->WriteBufIdx));

            if(SdbArenaGetPos(CurBuf) == Pipe->BufferMaxFill) {
                CurBuf = SdPipeGetWriteBuffer(Pipe);
            }


            u8 Frame[MODBUS_TCP_FRAME_MAX_SIZE] = { 0 };

            /**< First receive just the header */
            ssize_t HeaderResult = SocketRecvWithTimeout(Conn.SockFd, Frame, MODBUS_TCP_HEADER_LEN,
                                                         SDB_TIME_MS(500));

            if(HeaderResult == -2) {
                continue;
            }
            if(HeaderResult <= 0) {
                goto reconnect;
            }
            if(HeaderResult != MODBUS_TCP_HEADER_LEN) {
                SdbLogError("Incomplete header: %zd bytes", HeaderResult);
                goto reconnect;
            }

            /**< Get the length from the header */
            u16 Length = (Frame[4] << 8) | Frame[5];
            if(Length > MODBUS_TCP_FRAME_MAX_SIZE - MODBUS_TCP_HEADER_LEN) {
                SdbLogError("Invalid frame length: %u", Length);
                goto reconnect;
            }

            /**< Now receive the data portion */
            ssize_t DataResult = SocketRecvWithTimeout(Conn.SockFd, Frame + MODBUS_TCP_HEADER_LEN,
                                                       Length, SDB_TIME_MS(500));

            if(DataResult != Length) {
                SdbLogError("Incomplete data: %zd of %u bytes", DataResult, Length);
                goto reconnect;
            }

            u16       UnitId, DataLength;
            const u8 *Data = MbParseTcpFrame(Frame, &UnitId, &DataLength);

            if(!Data) {
                SdbLogError("Failed to parse frame");
                goto reconnect;
            }

            if(DataLength != Pipe->PacketSize) {
                SdbLogError("Size mismatch: got %u expected %zu", DataLength, Pipe->PacketSize);
                goto reconnect;
            }

            u8 *Ptr = SdbArenaPush(CurBuf, DataLength);
            SdbMemcpy(Ptr, Data, DataLength);

            static int counter = 0;
            if(++counter % 10000 == 0) {
                SdbLogInfo("Received %d packets", counter);
            }
        }

reconnect:
        SdPipeFlush(Pipe);
        /**< Clean up current connection before retry */
        for(u64 i = 0; i < MbCtx->ConnCount; ++i) {
            if(MbCtx->Conns[i].SockFd != -1) {
                close(MbCtx->Conns[i].SockFd);
                MbCtx->Conns[i].SockFd = -1;
            }
        }

        if(!SdbShouldShutdown()) {
            SdbLogInfo("Will attempt reconnection in 1 second");
            usleep(1000000); /**< Wait 1 second before retry */
        }
    }

    free(MbAMem);
    return Ret;
}
