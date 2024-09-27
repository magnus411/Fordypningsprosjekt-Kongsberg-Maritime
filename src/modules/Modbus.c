#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "Modbus.h"
#include "Socket.h"
#include "../CircularBuffer.h"
#define MODBUS_TCP_HEADER_LEN 7
#define MAX_MODBUS_TCP_FRAME  260

/**
 *
 * Resources:
 * @link https://modbus.org/docs/Modbus_Application_Protocol_V1_1b.pdf
 */

int
RecivedModbusFrame(int Sockfd, char *Buffer, int BufferSize)
{
    int TotalBytesRead = 0;

    while(TotalBytesRead < MODBUS_TCP_HEADER_LEN)
    {
        int BytesRead
            = recv(Sockfd, Buffer + TotalBytesRead, MODBUS_TCP_HEADER_LEN - TotalBytesRead, 0);
        if(BytesRead <= 0)
        {
            return BytesRead;
        }
        TotalBytesRead += BytesRead;
    }

    uint16_t Length = (Buffer[4] << 8) | Buffer[5]; // Length from Modbus TCP header

    int TotalFrameSize = MODBUS_TCP_HEADER_LEN + Length;

    if(TotalFrameSize > BufferSize)
    {
        printf("Frame too large for buffer. Total frame size: %d\n", TotalFrameSize);
        return -1;
    }

    while(TotalBytesRead < TotalFrameSize)
    {
        int BytesRead = recv(Sockfd, Buffer + TotalBytesRead, TotalFrameSize - TotalBytesRead, 0);
        if(BytesRead <= 0)
        {
            return BytesRead;
        }
        TotalBytesRead += BytesRead;
    }

    return TotalBytesRead;
}

void
ParseModbusTCPFrame(const char *Buf, int NumBytes, QueueItem *Item)
{
    if(NumBytes < MODBUS_TCP_HEADER_LEN)
    {
        printf("Invalid Modbus frame\n");
        return;
    }

    uint16_t TransactionId = (Buf[0] << 8) | Buf[1];
    uint16_t ProtocolId    = (Buf[2] << 8) | Buf[3];
    uint16_t Length        = (Buf[4] << 8) | Buf[5];
    uint8_t  UnitId        = Buf[6];
    uint8_t  FunctionCode  = Buf[7];

    /**
        printf("Transaction ID: %u\n", TransactionId);
        printf("Protocol ID: %u\n", ProtocolId);
        printf("Length: %u\n", Length);
        printf("Unit ID (Sensor ID): %u\n", UnitId);
        printf("Function Code (Protocol): 0x%02x\n", FunctionCode);
    */
    Item->UnitId   = UnitId;
    Item->Protocol = FunctionCode;

    if(FunctionCode == 0x03)
    {
        uint8_t ByteCount = Buf[8];
        printf("Byte Count: %u\n", ByteCount);

        // Ensure byte count is even (registers are 2 bytes each)
        if(ByteCount % 2 != 0)
        {
            printf("Warning: Odd byte count detected. Skipping this frame.\n");
            return;
        }

        if(ByteCount > MAX_DATA_LENGTH)
        {
            printf("Byte count exceeds maximum data length. Skipping this frame.\n");
            return;
        }

        memcpy(Item->Data, &Buf[9], ByteCount);
        Item->DataLength = ByteCount;
    }
    else
    {
        printf("Unsupported function code\n");
    }
}

void *
ModbusThread(void *arg)
{
    ModbusArgs modbus;
    memcpy(&modbus, arg, sizeof(ModbusArgs));

    CircularBuffer *Cb     = modbus.Cb;
    int             SockFd = CreateSocket(modbus.Ip, modbus.PORT);
    if(SockFd == -1)
    {
        pthread_exit(NULL);
    }

    char Buf[MAX_MODBUS_TCP_FRAME];
    while(1)
    {
        int NumBytes = RecivedModbusFrame(SockFd, Buf, sizeof(Buf));
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
            printf("Connection closed by server\n");
            close(SockFd);
            pthread_exit(NULL);
        }
        else
        {
            perror("recv");
            close(SockFd);
            pthread_exit(NULL);
        }
    }
}
