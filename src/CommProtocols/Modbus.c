#include "src/Common/Thread.h"
#include "src/Common/Time.h"
#include "tests/TestConstants.h"
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <src/Sdb.h>
SDB_LOG_REGISTER(Modbus);
SDB_THREAD_ARENAS_REGISTER(Modbus, 2);

#include <src/CommProtocols/CommProtocols.h>
#include <src/CommProtocols/Modbus.h>
#include <src/Common/CircularBuffer.h>
#include <src/Common/SensorDataPipe.h>
#include <src/Common/Socket.h>

#define MODBUS_TCP_HEADER_LEN 7
#define MAX_MODBUS_TCP_FRAME  260

/**
 *
 * Resources:
 * Modbus Application Protocol. Defines the Modbus spesifications.
 * @link https://modbus.org/docs/Modbus_Application_Protocol_V1_1b.pdf
 */

void
MbThreadArenasInit(void)
{
    SdbThreadArenasInit(Modbus);
}

ssize_t
MbReceiveTcpFrame(int Sockfd, u8 *Frame, size_t BufferSize)
{
    ssize_t TotalBytesRead = 0;

    while(TotalBytesRead < MODBUS_TCP_HEADER_LEN) {
        ssize_t BytesRead
            = recv(Sockfd, Frame + TotalBytesRead, MODBUS_TCP_HEADER_LEN - TotalBytesRead, 0);
        if(BytesRead <= 0) {
            return BytesRead;
        }
        TotalBytesRead += BytesRead;
    }

    u16 Length = (Frame[4] << 8) | Frame[5]; // Length from Modbus TCP header

    ssize_t TotalFrameSize = MODBUS_TCP_HEADER_LEN + Length;

    if(TotalFrameSize > BufferSize) {
        SdbLogDebug("Frame too large for buffer. Total frame size: %zd\n", TotalFrameSize);
        return -1;
    }

    while(TotalBytesRead < TotalFrameSize) {
        ssize_t BytesRead
            = recv(Sockfd, Frame + TotalBytesRead, TotalFrameSize - TotalBytesRead, 0);
        if(BytesRead <= 0) {
            return BytesRead;
        }
        TotalBytesRead += BytesRead;
    }

    return TotalBytesRead;
}

/**Modbus TCP frame structure:
 *
 * Resources:
 * @link https://en.wikipedia.org/wiki/Modbus#Public_function_codes
 *
 * | Transaction ID | Protocol ID | Length | Unit ID | Function Code | DataLength | Data  |
 * |----------------|-------------|--------|---------|---------------|----------|---------|
 * | 2 bytes        | 2 bytes     | 2 bytes| 1 byte  | 1 byte        | 1 byte   | n bytes |
 * ----------------------------------------------------------------------------------------
 */
const u8 *
MbParseFrame(const u8 *Frame, u16 *UnitId, u16 *FunctionCode, u16 *DataLength)
{
    // u16 TransactionId = (Buffer[0] << 8) | Buffer[1]; // TODO(ingar): Why are these not used?
    // u16 ProtocolId    = (Buffer[2] << 8) | Buffer[3];
    // u16 Length        = (Buffer[4] << 8) | Buffer[5];
    *UnitId       = Frame[6];
    *FunctionCode = Frame[7];
    *DataLength   = Frame[8];

    // Function code 0x03 is read multiple holding registers
    if(*FunctionCode != 0x03) {
        SdbLogError("Unsupported function code");
        return NULL;
    }

    SdbLogDebug("Modbus data length: %u", *DataLength);

    if(*DataLength > MAX_DATA_LENGTH) {
        SdbLogWarning("Byte count exceeds maximum data length. Skipping this frame.\n");
        return NULL;
    }

    return &Frame[9];
}

sdb_errno
MbPrepare(comm_protocol_api *Mb)
{
    modbus_ctx   *MbCtx = MB_CTX(Mb);
    mb_init_args *Args  = Mb->OptArgs;

    if(Args->PortCount != Args->IpCount) {
        SdbLogError("Mismatch in port count (%lu) and ip count (%lu)", Args->PortCount,
                    Args->IpCount);
        return -EINVAL;
    } else {
        MbCtx->ConnCount = Args->PortCount;
    }

    MbCtx->Ports   = SdbPushArray(&Mb->Arena, int, MbCtx->ConnCount);
    MbCtx->Ips     = SdbPushArray(&Mb->Arena, sdb_string, MbCtx->ConnCount);
    MbCtx->SockFds = SdbPushArray(&Mb->Arena, int, MbCtx->ConnCount);

    for(u64 i = 0; i < MbCtx->ConnCount; ++i) {
        MbCtx->Ports[i]   = Args->Ports[i];
        MbCtx->Ips[i]     = SdbStringDuplicate(&Mb->Arena, Args->Ips[i]);
        MbCtx->SockFds[i] = CreateSocket(MbCtx->Ips[i], MbCtx->Ports[i]);
        if(MbCtx->SockFds[i] == -1) {
            SdbLogError("Failed to create socket for sensor index %lu", i);
            return -1;
        } else {
            SdbLogDebug("Modbus thread %lu successfully connected to server %s:%d", i,
                        MbCtx->Ips[i], MbCtx->Ports[i]);
        }
    }


    return 0;
}

sdb_errno
MbMainLoop(comm_protocol_api *Mb)
{
    sdb_errno Ret = 0;

#if 0
    mb_thread_ctx    *MtCtx  = Thread->Args;
    sensor_data_pipe *Pipe   = MtCtx->Pipe;
    int               SockFd = MtCtx->SockFd;
    sdb_arena        *CurBuf = Pipe->Buffers[atomic_load(&Pipe->WriteBufIdx)];

    while(!SdbTCtlShouldStop(MtCtx->Control)) {
        SdbAssert((SdbArenaGetPos(CurBuf) <= Pipe->BufferMaxFill),
                  "Thread %ld: Pipe buffer overflow in buffer %u", Thread->pid,
                  atomic_load(&Pipe->WriteBufIdx));

        if(SdbArenaGetPos(CurBuf) == Pipe->BufferMaxFill) {
            CurBuf = SdPipeGetWriteBuffer(Pipe);
        }

        u8  Frame[MAX_MODBUS_TCP_FRAME];
        int RecvResult = ReceiveWithTimeout(SockFd, Frame, sizeof(Frame), 1);
        switch(RecvResult) {
            case -2: // Timeout
                continue;
            case -1: // Error
                SdbLogError("Thread %ld: Error during read operation for thread", Thread->pid);
                Ret = -1;
                goto exit;
                break;
            case 0: // Server disconnected
                SdbLogDebug("Thread %ld: Connection for thread losed by server", Thread->pid);
                Ret = 0;
                goto exit;
                break;
            default:
                { // Data received
                    u16       UnitId, FunctionCode, DataLength;
                    const u8 *Data = MbParseFrame(Frame, &UnitId, &FunctionCode, &DataLength);

                    SdbAssert(DataLength == Pipe->PacketSize,
                              "Thread %ld: Modbus packet size did not match expected, was %u "
                              "expected %zd",
                              Thread->pid, DataLength, Pipe->PacketSize);

                    u8 *Ptr = SdbArenaPush(CurBuf, DataLength);
                    SdbMemcpy(Ptr, Data, DataLength);
                }
                break;
        }
    }

exit:
    SdbLogDebug("Sensor thread for modbus thread %lu stopping with %s", Thread->pid,
                (Ret == 0) ? "success" : "error");
    SdPipeFlush(Pipe);
    close(SockFd);
    // Add error checking for the mark stopped call
    sdb_errno StopRet = SdbTCtlMarkStopped(MtCtx->Control);
    if(StopRet != 0) {
        SdbLogError("Thread %ld: Failed to mark thread as stopped", Thread->pid);
        return StopRet;
    }
#endif
    return Ret;
}
