#include "ModbusModule.h"
#include "SocketModule.h"
#include "CircularBuffer.h"
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define MODBUS_PORT 3490

void *
modbus_thread(void *arg)
{
    CircularBuffer *cb     = (CircularBuffer *)arg;
    int             sockfd = create_socket("127.0.0.1", MODBUS_PORT);
    if(sockfd == -1)
    {
        pthread_exit(NULL);
    }

    char buf[256];
    while(1)
    {
        int numbytes = recv(sockfd, buf, sizeof(buf) - 1, 0);
        if(numbytes > 0)
        {
            buf[numbytes] = '\0';
            QueueItem item;
            item.protocol  = 1;
            item.sensor_id = 123;
            item.data      = strdup(buf);

            InsertToBuffer(cb, &item);
            printf("Received: %s\n", cb->buffer[cb->tail].data);
        }
        else if(numbytes == 0)
        {
            continue;
        }
        else
        {
            perror("recv");
            close(sockfd);
            pthread_exit(NULL);
        }
    }
}
