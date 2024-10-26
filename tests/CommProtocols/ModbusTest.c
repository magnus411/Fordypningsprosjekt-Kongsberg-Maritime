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

#define READ_HOLDING_REGISTERS 0x03

typedef struct __attribute__((packed))
{
    pg_int4   Rpm;
    pg_float8 Torque;
    pg_float8 Power;

} power_shaft_data;

static void
GeneratePowerShaftData(power_shaft_data *Data)
{
    Data->Power  = rand() % 1000 + 100; //  Power in kW (random between 100 and 1000)
    Data->Torque = rand() % 500 + 50;   // Torque in Nm (random between 50 and 550)
    Data->Rpm    = rand() % 5000 + 500; // RPM (random between 500 and 5500)
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
SendModbusData(int NewFd, u16 UnitId)
{
    u8 ModbusFrame[MODBUS_TCP_HEADER_LEN + MAX_MODBUS_PDU_SIZE];

    u16 TransactionId = 1;
    u16 ProtocolId    = 0;
    u8  FunctionCode  = READ_HOLDING_REGISTERS;

    power_shaft_data Data = { 0 };

    u16 DataLength = sizeof(Data);
    u16 Length     = DataLength + 3;

    GeneratePowerShaftData(&Data);
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

    for(u64 i = 0; i < MODBUS_PACKET_COUNT; ++i) {
        if(SendModbusData(NewFd, UnitId) == -1) {
            SdbLogError("Failed to send Modbus data to client %s:%d, closing connection", ClientIp,
                        ntohs(ClientAddr.sin_port));

            close(NewFd);
            break;
        }
        SdbLogDebug("Successfully sent Modbus data to client %s:%d", ClientIp,
                    ntohs(ClientAddr.sin_port));

        // SdbSleep(SDB_TIME_MS(1));
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

sdb_errno
ModbusRunTest(comm_protocol_api *Modbus)
{
    modbus_ctx *Ctx    = Modbus->Ctx;
    int         SockFd = CreateSocket(Ctx->Ip, Ctx->PORT); // TODO(ingar): Move to init
    if(SockFd == -1) {
        SdbLogError("Failed to create socket");
        return -1;
    }

    u8 Buf[MAX_MODBUS_TCP_FRAME];
    for(u64 i = 0; i < MODBUS_PACKET_COUNT; ++i) {
        ssize_t NumBytes = RecivedModbusTCPFrame(SockFd, Buf, sizeof(Buf));
        if(NumBytes > 0) {
            queue_item Item;
            ParseModbusTCPFrame(Buf, NumBytes, &Item);
            SdPipeInsert(&Modbus->SdPipe, i % 4, &Item, sizeof(queue_item));
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
