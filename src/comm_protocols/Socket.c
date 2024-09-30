#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#define SDB_LOG_LEVEL 4

#include <SdbExtern.h>

SDB_LOG_REGISTER(Modbus);

#include <comm_protocols/Socket.h>

int
CreateSocket(const char *IpAddress, int Port)
{
    int                SockFd;
    struct sockaddr_in ServerAddr;

    if((SockFd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        SdbLogError("Error creating socket: %s ", errno);
        return -1;
    }

    memset(&ServerAddr, 0, sizeof(ServerAddr));
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_port   = htons(Port);

    if(inet_pton(AF_INET, IpAddress, &ServerAddr.sin_addr) <= 0) {
        SdbLogError("Invalid IP address format: %s", IpAddress);
        close(SockFd);
        return -1;
    }

    if(connect(SockFd, (struct sockaddr *)&ServerAddr, sizeof(ServerAddr)) == -1) {
        SdbLogError("Failed to connect to server (%s:%d): %s (errno: %d)", IpAddress,
                    ntohs(ServerAddr.sin_port), strerror(errno), errno);
        close(SockFd);
        return -1;
    }

    return SockFd;
}
