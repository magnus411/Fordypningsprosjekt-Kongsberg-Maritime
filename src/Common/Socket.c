#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <src/Sdb.h>
SDB_LOG_REGISTER(Socket);

#include <src/Common/Socket.h>

int
SocketCreate(const char *IpAddress, int Port)
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

// Returns:
//   >0: number of bytes received
//    0: server closed connection
//   -1: error occurred
//   -2: timeout occurred
int
SocketRecvWithTimeout(int SockFd, void *Buffer, size_t Length, sdb_timediff TimeoutDiff)
{
    fd_set         ReadFds;
    struct timeval Timeout;

    // Setup for select()
    FD_ZERO(&ReadFds);
    FD_SET(SockFd, &ReadFds);

    SdbTimeval(&Timeout, TimeoutDiff);
    // Wait for data or timeout
    int SelectResult = select(SockFd + 1, &ReadFds, NULL, NULL, &Timeout);

    if(SelectResult == -1) {
        // Select error
        SdbLogError("Select failed: %s", strerror(errno));
        return -1;
    } else if(SelectResult == 0) {
        // Timeout
        SdbLogError("Receive timeout after %lu milliseconds", SDB_TIME_TO_MS(TimeoutDiff));
        return -2;
    }

    // Data is available, perform the recv
    int RecvResult = recv(SockFd, Buffer, Length, 0);

    if(RecvResult == 0) {
        // Server closed connection
        SdbLogError("Server closed connection");
        return 0;
    } else if(RecvResult == -1) {
        SdbLogError("Recv failed: %s", strerror(errno));
        return -1;
    }

    return RecvResult;
}
