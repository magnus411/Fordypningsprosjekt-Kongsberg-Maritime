#include <src/Sdb.h>

SDB_LOG_DECLARE(Modbus);
SDB_THREAD_ARENAS_EXTERN(Modbus);

#include <src/CommProtocols/CommProtocols.h>
#include <src/CommProtocols/Modbus.h>
#include <src/Common/SensorDataPipe.h>
#include <src/Common/Socket.h>

sdb_errno
MbInit(comm_protocol_api *Mb)
{
    MbThreadArenasInit();
    SdbThreadArenasInitExtern(Modbus);
    sdb_arena *Scratch1 = SdbArenaBootstrap(&Mb->Arena, NULL, SdbKibiByte(512));
    sdb_arena *Scratch2 = SdbArenaBootstrap(&Mb->Arena, NULL, SdbKibiByte(512));
    SdbThreadArenasAdd(Scratch1);
    SdbThreadArenasAdd(Scratch2);

    Mb->Ctx = MbPrepareCtx(Mb);
    if(Mb->Ctx == NULL) {
        SdbLogError("Failed to initialize Modbus context");
        return -1;
    }

    return 0;
}

sdb_errno
MbRun(comm_protocol_api *Mb)
{
    sdb_errno Ret = 0;

    modbus_ctx         *MbCtx         = MB_CTX(Mb);
    sdb_thread_control *ModuleControl = Mb->ModuleControl;
    sdb_arena          *MbArena       = &Mb->Arena;
    mb_conn             Conn = MbCtx->Conns[0]; // TODO(ingar): Simplified to only use one for now

    sensor_data_pipe *Pipe = Mb->SdPipes[0]; // TODO(ingar): Simplified to only use one pipe for now
    sdb_arena        *CurBuf = Pipe->Buffers[atomic_load(&Pipe->WriteBufIdx)];

    while(!SdbTCtlShouldStop(ModuleControl)) {
        SdbAssert((SdbArenaGetPos(CurBuf) <= Pipe->BufferMaxFill),
                  "Thread %ld: Pipe buffer overflow in buffer %u", Thread->pid,
                  atomic_load(&Pipe->WriteBufIdx));

        if(SdbArenaGetPos(CurBuf) == Pipe->BufferMaxFill) {
            CurBuf = SdPipeGetWriteBuffer(Pipe);
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
                goto exit;
                break;
            default: // Data received
                {
                    u16       UnitId, FunctionCode, DataLength;
                    const u8 *Data = MbParseTcpFrame(Frame, &UnitId, &FunctionCode, &DataLength);

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
