#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/types.h>

#define SDB_LOG_LEVEL 4

#include "../SdbExtern.h"

SDB_LOG_REGISTER(Modbus);

#include "Modbus.h"
#include "Socket.h"
#include "../CircularBuffer.h"

#define MODBUS_TCP_HEADER_LEN 7
#define MAX_MODBUS_TCP_FRAME  260

/**
 *
 * Resources:
 * Modbus Application Protocol. Defines the Modbus spesifications.
 * @link https://modbus.org/docs/Modbus_Application_Protocol_V1_1b.pdf
 */

ssize_t
RecivedModbusFrame(int Sockfd, u8 *Buffer, size_t BufferSize)
{
    int TotalBytesRead = 0;

    while(TotalBytesRead < MODBUS_TCP_HEADER_LEN)
    {
        ssize_t BytesRead
            = recv(Sockfd, Buffer + TotalBytesRead, MODBUS_TCP_HEADER_LEN - TotalBytesRead, 0);
        if(BytesRead <= 0)
        {
            return BytesRead;
        }
        TotalBytesRead += BytesRead;
    }

    u16 Length = (Buffer[4] << 8) | Buffer[5]; // Length from Modbus TCP header

    ssize_t TotalFrameSize = MODBUS_TCP_HEADER_LEN + Length;

    if(TotalFrameSize > BufferSize)
    {
        SdbLogDebug("Frame too large for buffer. Total frame size: %d\n", TotalFrameSize);
        return -1;
    }

    while(TotalBytesRead < TotalFrameSize)
    {
        ssize_t BytesRead
            = recv(Sockfd, Buffer + TotalBytesRead, TotalFrameSize - TotalBytesRead, 0);
        if(BytesRead <= 0)
        {
            return BytesRead;
        }
        TotalBytesRead += BytesRead;
    }

    return TotalBytesRead;
}

sdb_errno
ParseModbusTCPFrame(const u8 *Buffer, int NumBytes, QueueItem *Item)
{
    if(NumBytes < MODBUS_TCP_HEADER_LEN)
    {
        printf("Invalid Modbus frame\n");
        return -1;
    }

    u16 TransactionId = (Buffer[0] << 8) | Buffer[1];
    u16 ProtocolId    = (Buffer[2] << 8) | Buffer[3];
    u16 Length        = (Buffer[4] << 8) | Buffer[5];
    u8  UnitId        = Buffer[6];
    u8  FunctionCode  = Buffer[7];

    u8 DataLength = Buffer[8];
    SdbLogDebug
        /**
            SdbLogDebug("Transaction ID: %u\n", TransactionId);
            SdbLogDebug("Protocol ID: %u\n", ProtocolId);
            SdbLogDebug("Length: %u\n", Length);
            SdbLogDebug("Unit ID (Sensor ID): %u\n", UnitId);
            SdbLogDebug("Function Code (Protocol): 0x%02x\n", FunctionCode);
        */
        Item->UnitId
        = UnitId;
    Item->Protocol = FunctionCode;

    // Function code 0x03 is read multiple holding registers
    if(FunctionCode != 0x03)
    {
        SdbLogDebug("Unsupported function code\n");
        return -1;
    }

    SdbLogDebug("Byte Count: %u\n", DataLength);

    // Ensure byte count is even (registers are 2 bytes each)
    if(DataLength % 2 != 0)
    {
        SdbLogDebug("Warning: Odd byte count detected. Skipping this frame.\n");
        return -1;
    }

    if(DataLength > MAX_DATA_LENGTH)
    {
        SdbLogDebug("Byte count exceeds maximum data length. Skipping this frame.\n");
        return -1;
    }

    memcpy(Item->Data, &Buffer[9], DataLength);
    Item->DataLength = DataLength;

    return 0;
}

void *
ModbusThread(void *arg)
{
    Modbus_Args modbus;
    memcpy(&modbus, arg, sizeof(Modbus_Args));

    circular_buffer *Cb     = modbus.Cb;
    int              SockFd = CreateSocket(modbus.Ip, modbus.PORT);
    if(SockFd == -1)
    {
        pthread_exit(NULL);
    }

    u8 Buf[MAX_MODBUS_TCP_FRAME];
    while(1)
    {
        ssize_t NumBytes = RecivedModbusFrame(SockFd, Buf, sizeof(Buf));
        if(NumBytes > 0)
        {
            QueueItem Item;
            ParseModbusTCPFrame(Buf, NumBytes, &Item);

            InsertToBuffer(Cb, &Item, sizeof(QueueItem));

            /*
                        QueueItem RecivedData;
                        ReadFromBuffer(Cb, &RecivedData, sizeof(QueueItem));

                        printf("Parsed QueueItem:\n");
                        printf("Data (Registers): ");
                        for(int i = 0; i < RecivedData.DataLength / 2; i++)
                        {
                            uint16_t RegisterValue
                                = (RecivedData.Data[i * 2] << 8) | RecivedData.Data[i * 2 + 1];
                            printf("%u ", RegisterValue);
                        }
                        printf("\n-----------------\n");
                        */
        }
        else if(NumBytes == 0)
        {
            SdbLogDebug("Connection closed by server\n");
            close(SockFd);
            pthread_exit(NULL);
        }
        else
        {
            SdbLogDebug("recv");
            close(SockFd);
            pthread_exit(NULL);
        }
    }
}
