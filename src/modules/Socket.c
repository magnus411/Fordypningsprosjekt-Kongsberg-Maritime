#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "Socket.h"

int
CreateSocket(const char *IpAddress, int Port)
{
    int                SockFd;
    struct sockaddr_in ServerAddr;

    if((SockFd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("client: socket");
        return -1;
    }

    memset(&ServerAddr, 0, sizeof(ServerAddr));
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_port   = htons(Port);

    if(inet_pton(AF_INET, IpAddress, &ServerAddr.sin_addr) <= 0)
    {
        perror("inet_pton");
        close(SockFd);
        return -1;
    }

    if(connect(SockFd, (struct sockaddr *)&ServerAddr, sizeof(ServerAddr)) == -1)
    {
        perror("client: connect");
        close(SockFd);
        return -1;
    }

    return SockFd;
}
