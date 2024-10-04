#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define SDB_LOG_LEVEL 4
#include <SdbExtern.h>

SDB_LOG_REGISTER(Modbus);

#include <comm_protocols/Modbus.h>
#include <comm_protocols/Socket.h>
#include <common/CircularBuffer.h>

#define MODBUS_TCP_HEADER_LEN 7
#define MAX_MODBUS_TCP_FRAME  260

/**
 *
 * Resources:
 * Modbus Application Protocol. Defines the Modbus spesifications.
 * @link https://modbus.org/docs/Modbus_Application_Protocol_V1_1b.pdf
 */

static ssize_t
RecivedModbusTCPFrame_(int Sockfd, u8 *Buffer, size_t BufferSize)
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
        SdbLogDebug("Frame too large for buffer. Total frame size: %d\n", TotalFrameSize);
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
ParseModbusTCPFrame_(const u8 *Buffer, int NumBytes, queue_item *Item)
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
    u8 UnitId       = Buffer[6];
    u8 FunctionCode = Buffer[7];

    u8 DataLength  = Buffer[8];
    Item->UnitId   = UnitId;
    Item->Protocol = FunctionCode;

    // Function code 0x03 is read multiple holding registers
    if(FunctionCode != 0x03) {
        SdbLogDebug("Unsupported function code\n");
        return -1;
    }

    SdbLogDebug("Byte Count: %u\n", DataLength);

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

void *
ModbusThread(void *arg)
{
    modbus_args modbus;
    memcpy(&modbus, arg, sizeof(modbus_args));

    circular_buffer *Cb     = modbus.Cb;
    int              SockFd = CreateSocket(modbus.Ip, modbus.PORT);
    if(SockFd == -1) {
        SdbLogError("Failed to create socket\n");
        pthread_exit(NULL);
    }

    u8 Buf[MAX_MODBUS_TCP_FRAME];
    while(1) {
        ssize_t NumBytes = RecivedModbusTCPFrame_(SockFd, Buf, sizeof(Buf));
        if(NumBytes > 0) {
            queue_item Item;
            ParseModbusTCPFrame_(Buf, NumBytes, &Item);

            CbInsert(Cb, &Item, sizeof(queue_item));
        } else if(NumBytes == 0) {
            SdbLogDebug("Connection closed by server");
            close(SockFd);
            pthread_exit(NULL);
        } else {
            SdbLogError("Error during read operattion. Closing connection\n");
            close(SockFd);
            pthread_exit(NULL);
        }
    }
}
