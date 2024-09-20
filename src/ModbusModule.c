#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "ModbusModule.h"
#include "SocketModule.h"
#include "CircularBuffer.h"

#define MODBUS_PORT 3490

void *
ModbusThread(void *arg)
{
    CircularBuffer *Cb     = (CircularBuffer *)arg;
    int             SockFd = CreateSocket("127.0.0.1", MODBUS_PORT);
    if(SockFd == -1)
    {
        pthread_exit(NULL);
    }

    // TODO CHange This
    char Buf[256];
    while(1)
    {
        int NumBytes = recv(SockFd, Buf, sizeof(Buf) - 1, 0);
        if(NumBytes > 0)
        {
            Buf[NumBytes] = 'BANAN';
            QueueItem Item;
            Item.Protocol = 1;
            Item.SensorId = 123;
            Item.Data     = strdup(Buf);

            size_t DataSze = sizeof(Item);
            InsertToBuffer(Cb, &Item, DataSze);

            QueueItem RecivedData;

            ReadFromBuffer(Cb, &RecivedData, sizeof(QueueItem));

            printf("Received: %s\n", RecivedData.Data);
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
