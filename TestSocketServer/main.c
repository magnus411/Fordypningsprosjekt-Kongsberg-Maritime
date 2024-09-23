#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>

#define MODBUS_TCP_HEADER_LEN 7
#define MAX_MODBUS_PDU_SIZE   253
#define MODBUS_PORT           3490
#define BACKLOG               5

#define READ_HOLDING_REGISTERS 0x03

void
GeneratePowerShaftData(uint8_t *DataBuffer)
{
    // Simulated power shaft data (32-bit values split into two 16-bit registers)
    uint32_t Power  = rand() % 1000 + 100; //  Power in kW (random between 100 and 1000)
    uint32_t Torque = rand() % 500 + 50;   // Torque in Nm (random between 50 and 550)
    uint32_t Rpm    = rand() % 5000 + 500; // RPM (random between 500 and 5500)

    // Store data in 16-bit register format (big-endian)
    DataBuffer[0] = (Power >> 8) & 0xFF;  // Power high byte
    DataBuffer[1] = Power & 0xFF;         // Power low byte
    DataBuffer[2] = (Torque >> 8) & 0xFF; // Torque high byte
    DataBuffer[3] = Torque & 0xFF;        // Torque low byte
    DataBuffer[4] = (Rpm >> 8) & 0xFF;    // RPM high byte
    DataBuffer[5] = Rpm & 0xFF;           // RPM low byte
}

void
GenerateModbusTcpFrame(uint8_t *Buffer, uint16_t TransactionId, uint16_t ProtocolId,
                       uint16_t Length, uint8_t UnitId, uint8_t FunctionCode, uint8_t *Data,
                       uint16_t DataLength)
{
    Buffer[0] = (TransactionId >> 8) & 0xFF;
    Buffer[1] = TransactionId & 0xFF;
    Buffer[2] = (ProtocolId >> 8) & 0xFF;
    Buffer[3] = ProtocolId & 0xFF;
    Buffer[4] = (Length >> 8) & 0xFF;
    Buffer[5] = Length & 0xFF;
    Buffer[6] = UnitId;
    Buffer[7] = FunctionCode;
    Buffer[8] = DataLength;
    memcpy(&Buffer[9], Data, DataLength);
}

int
SendModbusData(int NewFd)
{
    uint8_t ModbusFrame[MODBUS_TCP_HEADER_LEN + MAX_MODBUS_PDU_SIZE];

    uint16_t TransactionId = 1;
    uint16_t ProtocolId    = 0;
    uint16_t UnitId        = 1;
    uint8_t  FunctionCode  = READ_HOLDING_REGISTERS;

    uint8_t Data[6];
    GeneratePowerShaftData(Data);

    uint16_t DataLength = sizeof(Data);

    uint16_t Length = DataLength + 3;

    GenerateModbusTcpFrame(ModbusFrame, TransactionId, ProtocolId, Length, UnitId, FunctionCode,
                           Data, DataLength);

    ssize_t SendResult = send(NewFd, ModbusFrame, MODBUS_TCP_HEADER_LEN + Length, 0);
    if(SendResult == -1)
    {
        perror("send");
        return -1;
    }

    return 0;
}

int
main(void)
{
    srand(time(NULL));

    int                SockFd, NewFd;
    struct sockaddr_in ServerAddr, ClientAddr;
    socklen_t          SinSize;
    char               ClientIp[INET6_ADDRSTRLEN];

    // Create the socket
    SockFd = socket(AF_INET, SOCK_STREAM, 0);
    if(SockFd == -1)
    {
        perror("socket");
        exit(1);
    }

    ServerAddr.sin_family      = AF_INET;
    ServerAddr.sin_port        = htons(MODBUS_PORT);
    ServerAddr.sin_addr.s_addr = INADDR_ANY;
    memset(&(ServerAddr.sin_zero), '\0', 8);

    if(bind(SockFd, (struct sockaddr *)&ServerAddr, sizeof(struct sockaddr)) == -1)
    {
        perror("bind");
        close(SockFd);
        exit(1);
    }

    if(listen(SockFd, BACKLOG) == -1)
    {
        perror("listen");
        close(SockFd);
        exit(1);
    }

    printf("Server: waiting for connections...\n");

    while(1)
    {
        SinSize = sizeof(ClientAddr);
        NewFd   = accept(SockFd, (struct sockaddr *)&ClientAddr, &SinSize);
        if(NewFd == -1)
        {
            perror("accept");
            continue;
        }

        inet_ntop(ClientAddr.sin_family, &(ClientAddr.sin_addr), ClientIp, sizeof(ClientIp));
        printf("Server: got connection from %s\n", ClientIp);

        while(1)
        {
            if(SendModbusData(NewFd) == -1)
            {
                close(NewFd);
                break;
            }
            sleep(1);
        }
    }

    close(SockFd);
    return 0;
}
