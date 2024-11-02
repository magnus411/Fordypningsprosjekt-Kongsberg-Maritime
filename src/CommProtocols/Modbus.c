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
#include <src/Common/Thread.h>

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
MbParseTcpFrame(const u8 *Frame, u16 *UnitId, u16 *FunctionCode, u16 *DataLength)
{
    // TODO(ingar): Why are these not used?
    // u16 TransactionId = (Buffer[0] << 8) | Buffer[1];
    // u16 ProtocolId    = (Buffer[2] << 8) | Buffer[3];
    // u16 Length        = (Buffer[4] << 8) | Buffer[5];
    *UnitId       = Frame[6];
    *FunctionCode = Frame[7];
    *DataLength   = Frame[8];

    // Function code 0x03 is read multiple holding registers
    if(*FunctionCode != 0x03) {
        SdbLogError("Unsupported function code 0x03 found in frame");
        return NULL;
    }

    if(*DataLength > MAX_DATA_LENGTH) {
        SdbLogWarning("Byte count exceeds maximum data length. Skipping this frame.\n");
        return NULL;
    }

    return &Frame[9];
}

modbus_ctx *
MbPrepareCtx(comm_protocol_api *Mb)
{
    mb_init_args *Args = Mb->OptArgs;

    if(Args->PortCount != Args->IpCount) {
        SdbLogError("Mismatch in port count (%lu) and ip count (%lu)", Args->PortCount,
                    Args->IpCount);
        return NULL;
    }

    modbus_ctx *MbCtx = SdbPushStruct(&Mb->Arena, modbus_ctx);
    MbCtx->ConnCount  = Args->PortCount;
    mb_conn *Conns    = SdbPushArray(&Mb->Arena, mb_conn, MbCtx->ConnCount);
    MbCtx->Conns      = Conns;

    for(u64 i = 0; i < MbCtx->ConnCount; ++i) {
        Conns[i].Port   = Args->Ports[i];
        Conns[i].Ip     = SdbStringDuplicate(&Mb->Arena, Args->Ips[i]);
        Conns[i].SockFd = SocketCreate(Conns[i].Ip, Conns[i].Port);
        if(Conns[i].SockFd == -1) {
            SdbLogError("Failed to create socket for sensor index %lu", i);
            return NULL;
        } else {
            SdbLogDebug("Modbus thread %lu successfully connected to server %s:%d", i, Conns[i].Ip,
                        Conns[i].Port);
        }
    }

    return MbCtx;
}
