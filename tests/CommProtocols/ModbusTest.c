#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <src/Sdb.h>

SDB_LOG_REGISTER(TestModbus);

#include <src/CommProtocols/Modbus.h>
#include <src/Common/CircularBuffer.h>
#include <src/Common/Socket.h>
#include <src/Common/Thread.h>
#include <src/Common/Time.h>
#include <src/DatabaseSystems/Postgres.h>
#include <tests/TestConstants.h>

#define MODBUS_TCP_HEADER_LEN 7
#define MAX_MODBUS_PDU_SIZE   253
#define MAX_MODBUS_TCP_FRAME  260
#define BACKLOG               5
#define PACKET_HZ           10



#define READ_HOLDING_REGISTERS 0x03

typedef struct __attribute__((packed))
{
    pg_int4   ID;
    pg_float8 TIME;
    pg_float8 Rpm;
    pg_float8 Torque;
    pg_float8 Power;
    pg_float8 Peak_Peak_PFS;
} power_shaft_data;

static void
GeneratePowerShaftData(power_shaft_data *Data, int ID)
{
    time_t CurrentTime = time(NULL);
    struct tm *TmInfo = localtime(&CurrentTime);

    if (rand() % 10 < 2) {
        Data->ID = ID;
        Data->TIME = (pg_float8)difftime(CurrentTime, 0);
        Data->Rpm = 0;
        Data->Torque = 0;
        Data->Power = 0;
        Data->Peak_Peak_PFS = 0;
    } else {
        Data->ID = ID;
        Data->TIME = (pg_float8)difftime(CurrentTime, 0);
        Data->Rpm = ((rand() % 100) + 30.0) * sin(ID * 0.1);  // RPM with variation
        Data->Torque = ((rand() % 600) + 100.0) * 0.8;  // Torque (Nm)
        Data->Power = Data->Rpm * Data->Torque / 9.5488;  // Derived power (kW)
        Data->Peak_Peak_PFS = ((rand() % 50) + 25.0) * cos(ID * 0.1);  // PFS variation
    }
}

static void
GenerateModbusTcpFrame(u8 *Buffer, u16 TransactionId, u16 ProtocolId, u16 Length, u8 UnitId,
                       u16 FunctionCode, u8 *Data, u16 DataLength)
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

sdb_errno
SendModbusData(int NewFd, u16 UnitId, int ID)
{
    u8 ModbusFrame[MODBUS_TCP_HEADER_LEN + MAX_MODBUS_PDU_SIZE];

    u16 TransactionId = 1;
    u16 ProtocolId    = 0;
    u8  FunctionCode  = READ_HOLDING_REGISTERS;

    power_shaft_data Data = { 0 };

    u16 DataLength = sizeof(Data);
    u16 Length     = DataLength + 3;

    GeneratePowerShaftData(&Data, ID);
    GenerateModbusTcpFrame(ModbusFrame, TransactionId, ProtocolId, Length, UnitId, FunctionCode,
                           (u8 *)&Data, DataLength);

    ssize_t SendResult = send(NewFd, ModbusFrame, MODBUS_TCP_HEADER_LEN + Length, 0);

    return SendResult;
}


sdb_errno
RunModbusTestServer(sdb_thread *Thread)
{
    SdbLogInfo("Running Modbus Test Server over Unix Sockets ...");

    srand(time(NULL));

    int                SockFd, NewFd;
    struct sockaddr_in ServerAddr, ClientAddr;
    socklen_t          SinSize;
    char               ClientIp[INET6_ADDRSTRLEN];

    int Port   = MODBUS_PORT;
    u16 UnitId = 1;

    SockFd = socket(AF_INET, SOCK_STREAM, 0);
    if(SockFd == -1) {
        SdbLogError("Failed to create socket: %s (errno: %d)", strerror(errno), errno);
        return SockFd;
    }

    ServerAddr.sin_family      = AF_INET;
    ServerAddr.sin_port        = htons(Port);
    ServerAddr.sin_addr.s_addr = INADDR_ANY;
    SdbMemset(&(ServerAddr.sin_zero), '\0', 8);

    if(bind(SockFd, (struct sockaddr *)&ServerAddr, sizeof(struct sockaddr)) == -1) {
        SdbLogError("Failed to bind socket (address: %s, port: %d): %s (errno: %d)",
                    inet_ntoa(ServerAddr.sin_addr), ntohs(ServerAddr.sin_port), strerror(errno),
                    errno);
        close(SockFd);
        return -1;
    }

    if(listen(SockFd, BACKLOG) == -1) {
        SdbLogError("Failed to listen on socket: %s (errno: %d)", strerror(errno), errno);
        close(SockFd);
        return -1;
    }

    SdbLogDebug("Server: waiting for connections on port %d...", Port);
    SdbBarrierWait(Thread->Args);

    SinSize = sizeof(ClientAddr);
    NewFd   = accept(SockFd, (struct sockaddr *)&ClientAddr, &SinSize);
    if(NewFd == -1) {
        SdbLogError("Error accepting connection: %s (errno: %d)", strerror(errno), errno);
        return -1;
    }

    inet_ntop(ClientAddr.sin_family, &(ClientAddr.sin_addr), ClientIp, sizeof(ClientIp));
    SdbLogInfo("Server: accepted connection from %s:%d", ClientIp, ntohs(ClientAddr.sin_port));

    i64 Counter = 0;
    while(Counter++ < MODBUS_PACKET_COUNT) {
        if(SendModbusData(NewFd, UnitId, Counter) == -1) {
            SdbLogError("Failed to send Modbus data to client %s:%d, closing connection", ClientIp,
                        ntohs(ClientAddr.sin_port));

            close(NewFd);
            break;
        }
        SdbLogDebug("Successfully sent Modbus data to client %s:%d", ClientIp,
                    ntohs(ClientAddr.sin_port));

        SdbSleep(SDB_TIME_S(1/PACKET_HZ));
    }

    close(SockFd);
    return 0;
}


sdb_errno
ModbusInitTest(comm_protocol_api *Modbus)
{
    modbus_ctx *Ctx = SdbPushStruct(&Modbus->Arena, modbus_ctx);
    Ctx->PORT       = MODBUS_PORT;
    strncpy(Ctx->Ip, (char *)Modbus->OptArgs, 10);
    Modbus->Ctx = Ctx;

    return 0;
}

// TODO(ingar): Remove when tests are properly up and running
#include <tests/TestConstants.h>
sdb_errno
ModbusRunTest(comm_protocol_api *Modbus)
{
    modbus_ctx *Ctx    = Modbus->Ctx;
    int         SockFd = CreateSocket(Ctx->Ip, Ctx->PORT); // TODO(ingar): Move to init
    if(SockFd == -1) {
        SdbLogError("Failed to create socket");
        return -1;
    }

    u8  Buf[MAX_MODBUS_TCP_FRAME];
    u64 Counter = 0;
    while(Counter++ < MODBUS_PACKET_COUNT) {
        ssize_t NumBytes = RecivedModbusTCPFrame(SockFd, Buf, sizeof(Buf));
        if(NumBytes > 0) {
            queue_item Item;
            ParseModbusTCPFrame(Buf, NumBytes, &Item);
            SdPipeInsert(&Modbus->SdPipe, Counter % 4, &Item, sizeof(queue_item));
        } else if(NumBytes == 0) {
            SdbLogDebug("Connection closed by server");
            close(SockFd); // TODO(ingar): Move to finalize
            return -1;
        } else {
            SdbLogError("Error during read operattion. Closing connection");
            close(SockFd);
            return -1;
        }
    }

    return 0;
}

sdb_errno
ModbusFinalizeTest(comm_protocol_api *Modbus)
{
    return 0;
}
