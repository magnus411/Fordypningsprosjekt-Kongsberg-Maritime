#include "SocketModule.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

int
create_socket(const char *ip_address, int port)
{
    int                sockfd;
    struct sockaddr_in server_addr;

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("client: socket");
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(port);

    if(inet_pton(AF_INET, ip_address, &server_addr.sin_addr) <= 0)
    {
        perror("inet_pton");
        close(sockfd);
        return -1;
    }

    if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("client: connect");
        close(sockfd);
        return -1;
    }

    return sockfd;
}
