#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <src/Sdb.h>
SDB_LOG_REGISTER(Modbus);
SDB_THREAD_ARENAS_REGISTER(Modbus, 2);

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
MbParseTcpFrame(const u8 *Frame, u16 *UnitId, u16 *DataLength)
{
    // Parse the Length field from the Modbus TCP header (bytes 4-5)
    u16 Length = (Frame[4] << 8) | Frame[5];

    // Parse the Unit ID (byte 6)
    *UnitId = Frame[6];

    // The actual data length is in byte 8
    *DataLength = Frame[8];

    // Verify that Length = DataLength + 3 (accounting for UnitID, FunctionCode, and ByteCount)
    if(Length != *DataLength + 3) {
        SdbLogWarning("Inconsistent Modbus frame lengths: Length=%u, DataLength=%u", Length,
                      *DataLength);
        return NULL;
    }

    return &Frame[9]; // Return pointer to start of data
}


modbus_ctx *
MbPrepareCtx(sdb_arena *MbArena)
{
    modbus_ctx *MbCtx = SdbPushStruct(MbArena, modbus_ctx);
    MbCtx->ConnCount  = 1;
    MbCtx->Conns      = SdbPushArray(MbArena, mb_conn, MbCtx->ConnCount);

    mb_conn *Conns = MbCtx->Conns;
    for(u64 i = 0; i < MbCtx->ConnCount; ++i) {
        Conns[i].Port   = 50123;
        Conns[i].Ip     = SdbStringMake(MbArena, "127.0.0.1");
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
//        Conns[i].Ip     = SdbStringMake(MbArena, "10.110.0.2");
