#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <getopt.h>

#define SDB_LOG_LEVEL 4
#include "../sdb.h"
#include "../ModbusData.h"

SDB_LOG_REGISTER(RunSocketTestServer);

#define MODBUS_TCP_HEADER_LEN 7
#define MAX_MODBUS_PDU_SIZE   253
#define BACKLOG               5

#define READ_HOLDING_REGISTERS 0x03

sdb_errno
SendModbusData(int NewFd, u16 UnitId)
{
    u8 ModbusFrame[MODBUS_TCP_HEADER_LEN + MAX_MODBUS_PDU_SIZE];

    u16 TransactionId = 1;
    u16 ProtocolId    = 0;
    u8  FunctionCode  = READ_HOLDING_REGISTERS;

    u8 Data[6];

    u16 DataLength = sizeof(Data);
    u16 Length     = DataLength + 3;

    GeneratePowerShaftData(Data);
    GenerateModbusTcpFrame(ModbusFrame, TransactionId, ProtocolId, Length, UnitId, FunctionCode,
                           Data, DataLength);

    ssize_t SendResult = send(NewFd, ModbusFrame, MODBUS_TCP_HEADER_LEN + Length, 0);

    return SendResult;
}

sdb_errno
RunSocketTestServer(int argc, char **argv)
{
    srand(time(NULL));

    int                SockFd, NewFd;
    struct sockaddr_in ServerAddr, ClientAddr;
    socklen_t          SinSize;
    char               ClientIp[INET6_ADDRSTRLEN];

    int Port   = 3490;
    u16 UnitId = 1;
    int Speed  = 1;

    int Opt;
    while((Opt = getopt(argc, argv, "p:u:s:")) != -1)
    {
        switch(Opt)
        {
            case 'p':
                Port = atoi(optarg);
                break;
            case 'u':
                UnitId = (u16)atoi(optarg);
                break;
            case 's':
                Speed = atoi(optarg);
                break;
            default:
                SdbLogError("Usage: %s [-p port] [-u unitId] [-s speed]", argv[0]);
                return -EINVAL;
        }
    }

    SockFd = socket(AF_INET, SOCK_STREAM, 0);
    if(SockFd == -1)
    {
        SdbLogError("Failed to create socket: %s (errno: %d)", strerror(errno), errno);
        return -1;
    }

    ServerAddr.sin_family      = AF_INET;
    ServerAddr.sin_port        = htons(Port);
    ServerAddr.sin_addr.s_addr = INADDR_ANY;
    memset(&(ServerAddr.sin_zero), '\0', 8);

    if(bind(SockFd, (struct sockaddr *)&ServerAddr, sizeof(struct sockaddr)) == -1)
    {
        SdbLogError("Failed to bind socket (address: %s, port: %d): %s (errno: %d)",
                    inet_ntoa(ServerAddr.sin_addr), ntohs(ServerAddr.sin_port), strerror(errno),
                    errno);
        close(SockFd);
        return -1;
        |
    }

    if(listen(SockFd, BACKLOG) == -1)
    {
        SdbLogError("Failed to listen on socket: %s (errno: %d)", strerror(errno), errno);
        close(SockFd);
        return -1;
    }

    SdbLogDebug("Server: waiting for connections on port %d...\n", Port);

    while(1)
    {
        SinSize = sizeof(ClientAddr);
        NewFd   = accept(SockFd, (struct sockaddr *)&ClientAddr, &SinSize);
        if(NewFd == -1)
        {
            SdbLogError("Error accepting connection: %s (errno: %d)", strerror(errno), errno);
            continue;
        }

        inet_ntop(ClientAddr.sin_family, &(ClientAddr.sin_addr), ClientIp, sizeof(ClientIp));
        SdbLogInfo("Server: accepted connection from %s:%d", ClientIp, ntohs(ClientAddr.sin_port));

        while(1)
        {
            if(SendModbusData(NewFd, UnitId) == -1)
            {
                SdbLogError("Failed to send Modbus data to client %s:%d, closing connection",
                            ClientIp, ntohs(ClientAddr.sin_port));

                close(NewFd);
                break;
            }
            SdbLogDebug("Successfully sent Modbus data to client %s:%d", ClientIp,
                        ntohs(ClientAddr.sin_port));

            sleep(Speed);
        }
    }

    close(SockFd);
    return 0;
}
