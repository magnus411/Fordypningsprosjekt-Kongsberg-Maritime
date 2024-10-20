#include "src/CommProtocols/CommProtocols.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <src/Sdb.h>
SDB_LOG_REGISTER(Modbus);

#include <src/CommProtocols/Modbus.h>
#include <src/CommProtocols/Socket.h>
#include <src/Common/CircularBuffer.h>

#define MODBUS_TCP_HEADER_LEN 7
#define MAX_MODBUS_TCP_FRAME  260

/**
 *
 * Resources:
 * Modbus Application Protocol. Defines the Modbus spesifications.
 * @link https://modbus.org/docs/Modbus_Application_Protocol_V1_1b.pdf
 */

static ssize_t
RecivedModbusTCPFrame(int Sockfd, u8 *Buffer, size_t BufferSize)
{
    ssize_t TotalBytesRead = 0;

    while(TotalBytesRead < MODBUS_TCP_HEADER_LEN) {
        ssize_t BytesRead
            = recv(Sockfd, Buffer + TotalBytesRead, MODBUS_TCP_HEADER_LEN - TotalBytesRead, 0);
        if(BytesRead <= 0) {
            return BytesRead;
        }
        TotalBytesRead += BytesRead;
    }

    u16 Length = (Buffer[4] << 8) | Buffer[5]; // Length from Modbus TCP header

    ssize_t TotalFrameSize = MODBUS_TCP_HEADER_LEN + Length;

    if(TotalFrameSize > BufferSize) {
        SdbLogDebug("Frame too large for buffer. Total frame size: %zd\n", TotalFrameSize);
        return -1;
    }

    while(TotalBytesRead < TotalFrameSize) {
        ssize_t BytesRead
            = recv(Sockfd, Buffer + TotalBytesRead, TotalFrameSize - TotalBytesRead, 0);
        if(BytesRead <= 0) {
            return BytesRead;
        }
        TotalBytesRead += BytesRead;
    }

    return TotalBytesRead;
}

static sdb_errno
ParseModbusTCPFrame(const u8 *Buffer, int NumBytes, queue_item *Item)
{
    if(NumBytes < MODBUS_TCP_HEADER_LEN) {
        SdbLogError("Invalid Modbus frame\n");
        return -EINVAL;
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
    // u16 TransactionId = (Buffer[0] << 8) | Buffer[1]; // TODO(ingar): Why are these not used?
    // u16 ProtocolId    = (Buffer[2] << 8) | Buffer[3];
    // u16 Length        = (Buffer[4] << 8) | Buffer[5];
    u16 UnitId       = Buffer[6];
    u16 FunctionCode = Buffer[7];
    u16 DataLength   = Buffer[8];
    Item->UnitId     = UnitId;
    Item->Protocol   = FunctionCode;

    // Function code 0x03 is read multiple holding registers
    if(FunctionCode != 0x03) {
        SdbLogDebug("Unsupported function code\n");
        return -1;
    }

    SdbLogDebug("Byte Count: %u", DataLength);

    // Ensure byte count is even (registers are 2 bytes each)
    if(DataLength % 2 != 0) {
        SdbLogWarning("Warning: Odd byte count detected. Skipping this frame.\n");
        return -1;
    }

    if(DataLength > MAX_DATA_LENGTH) {
        SdbLogWarning("Byte count exceeds maximum data length. Skipping this frame.\n");
        return -1;
    }

    memcpy(Item->Data, &Buffer[9], DataLength);
    Item->DataLength = DataLength;

    return 0;
}

sdb_errno
ModbusInit(comm_protocol_api *Modbus, void *OptArgs)
{
    modbus_ctx *Ctx = SdbPushStruct(&Modbus->Arena, modbus_ctx);
    Ctx->PORT       = 3490;
    strncpy(Ctx->Ip, "127.0.0.1", 10);
    Modbus->Ctx = Ctx;

    return 0;
}

sdb_errno
ModbusRun(comm_protocol_api *Modbus)
{
    modbus_ctx *Ctx    = Modbus->Ctx;
    int         SockFd = CreateSocket(Ctx->Ip, Ctx->PORT);
    if(SockFd == -1) {
        SdbLogError("Failed to create socket");
        return -1;
    }

    u8  Buf[MAX_MODBUS_TCP_FRAME];
    u64 Counter = 0;
    while(Counter++ < 1e5) {
        ssize_t NumBytes = RecivedModbusTCPFrame(SockFd, Buf, sizeof(Buf));
        if(NumBytes > 0) {
            queue_item Item;
            ParseModbusTCPFrame(Buf, NumBytes, &Item);
            SdPipeInsert(&Modbus->SdPipe, Counter % 4, &Item, sizeof(queue_item));
        } else if(NumBytes == 0) {
            SdbLogDebug("Connection closed by server");
            close(SockFd);
            return -1;
        } else {
            SdbLogError("Error during read operattion. Closing connection");
            close(SockFd);
            return -1;
        }
    }

    return 0;
}

sdb_errno
ModbusFinalize(comm_protocol_api *Modbus)
{
    return 0;
}
