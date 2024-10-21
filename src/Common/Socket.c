#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <src/Sdb.h>
SDB_LOG_REGISTER(Socket);

#include <src/Common/Socket.h>

int
CreateSocket(const char *IpAddress, int Port)
{
    int                SockFd;
    struct sockaddr_in ServerAddr;

    if((SockFd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        SdbLogError("Error creating socket: %s", strerror(errno));
        return -1;
    }

    SdbMemset(&ServerAddr, 0, sizeof(ServerAddr));
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_port   = htons(Port);

    if(inet_pton(AF_INET, IpAddress, &ServerAddr.sin_addr) <= 0) {
        SdbLogError("Invalid IP address format: %s", IpAddress);
        close(SockFd);
        return -1;
    }

    if(connect(SockFd, (struct sockaddr *)&ServerAddr, sizeof(ServerAddr)) == -1) {
        SdbLogError("Failed to connect to server %s:%d, errno: %s", IpAddress,
                    ntohs(ServerAddr.sin_port), strerror(errno));
        close(SockFd);
        return -1;
    }

    return SockFd;
}
