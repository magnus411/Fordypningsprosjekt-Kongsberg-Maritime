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

// Returns:
//   >0: number of bytes received
//    0: server closed connection
//   -1: error occurred
//   -2: timeout occurred
int
ReceiveWithTimeout(int SockFd, void *Buffer, size_t Length, int TimeoutSecs)
{
    fd_set         ReadFds;
    struct timeval Timeout;

    // Setup for select()
    FD_ZERO(&ReadFds);
    FD_SET(SockFd, &ReadFds);

    // Set timeout
    Timeout.tv_sec  = TimeoutSecs;
    Timeout.tv_usec = 0;

    // Wait for data or timeout
    int SelectResult = select(SockFd + 1, &ReadFds, NULL, NULL, &Timeout);

    if(SelectResult == -1) {
        // Select error
        SdbLogError("Select failed: %s", strerror(errno));
        return -1;
    } else if(SelectResult == 0) {
        // Timeout
        SdbLogError("Receive timeout after %d seconds", TimeoutSecs);
        return -2;
    }

    // Data is available, perform the recv
    int RecvResult = recv(SockFd, Buffer, Length, 0);

    if(RecvResult == 0) {
        // Server closed connection
        SdbLogError("Server closed connection");
        return 0;
    } else if(RecvResult == -1) {
        SdbLogError("Recv failed: %s", strerror(errno));
        return -1;
    }

    return RecvResult;
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
MbPrepareThreads(comm_protocol_api *Mb)
{
    modbus_ctx   *MbCtx = MB_CTX(Mb);
    mb_init_args *Args  = Mb->OptArgs;
    for(u64 t = 0; t < Mb->SensorCount; ++t) {
        mb_thread_ctx *MtCtx = SdbPushStruct(&Mb->Arena, mb_thread_ctx *);

        MtCtx->Pipe    = Mb->SdPipes[t];
        MtCtx->Control = &MbCtx->ThreadControls[t];
        MtCtx->Port    = Args->Ports[t];
        MtCtx->Ip      = SdbStringDuplicate(&Mb->Arena, Args->Ips[t]);
        MtCtx->SockFd  = CreateSocket(MtCtx->Ip, MtCtx->Port);

        if(MtCtx->SockFd == -1) {
            SdbLogError("Failed to create for sensor index %lu", t);
            return -1;
        } else {
            SdbLogDebug("Modbus thread %lu successfully connected to server %s:%d", t, MtCtx->Ip,
                        MtCtx->Port);
        }

        MbCtx->ThreadContexts[t] = MtCtx;
    }

    return 0;
}

sdb_errno
MbSensorThread(sdb_thread *Thread)
{
    sdb_errno Ret = 0;

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

    return Ret;
}
