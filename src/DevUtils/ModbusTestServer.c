#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <src/Sdb.h>

SDB_LOG_REGISTER(ModbusTestServer);

#include <src/CommProtocols/Modbus.h>
#include <src/Common/CircularBuffer.h>
#include <src/Common/Socket.h>
#include <src/Common/Thread.h>
#include <src/Common/Time.h>
#include <src/DatabaseSystems/Postgres.h>
#include <src/DevUtils/TestConstants.h>

#define BACKLOG     5
#define PACKET_FREQ 1e5

static inline void
GenerateShaftPowerData(shaft_power_data *Data)
{
    static i64 Id          = 1;
    static i64 Rpm         = 1;
    static i64 Torque      = 1;
    static i64 Power       = 1;
    static i64 PeakPeakPfs = 1;

    Data->PacketId    = Id++;
    Data->Time        = time(NULL);
    Data->Rpm         = Rpm++;
    Data->Torque      = Torque++;
    Data->Power       = Power++;
    Data->PeakPeakPfs = PeakPeakPfs++;
}

static void
GenerateShaftPowerDataRandom(shaft_power_data *Data)
{
    time_t     CurrentTime = time(NULL);
    static i64 Id          = 0;

    Data->PacketId    = Id++;
    Data->Time        = CurrentTime;
    Data->Rpm         = ((rand() % 100) + 30.0) * sin(Id * 0.1); // RPM with variation
    Data->Torque      = ((rand() % 600) + 100.0) * 0.8;          // Torque (Nm)
    Data->Power       = Data->Rpm * Data->Torque / 9.5488;       // Derived power (kW)
    Data->PeakPeakPfs = ((rand() % 50) + 25.0) * cos(Id * 0.1);  // PFS variation
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
    SdbMemcpy(&Buffer[9], Data, DataLength);
}


static inline sdb_errno
SendModbusData(int NewFd)
{
    // NOTE(ingar): By default everything is set to 1
    static u8               ModbusFrame[MODBUS_TCP_FRAME_MAX_SIZE] = { 1 };
    static const u16        DataLength                             = sizeof(shaft_power_data);
    static const u16        Length                                 = DataLength + 3;
    static shaft_power_data SpData                                 = {
                                        .PacketId = 1, .Time = 783883485000000, .Rpm = 1, .Torque = 1, .Power = 1, .PeakPeakPfs = 1
    };

    u16 TransactionId = 1;
    u16 ProtocolId    = 1;
    u8  FunctionCode  = 1;

    ModbusFrame[8] = DataLength;
    SdbMemcpy(&ModbusFrame[9], &SpData, DataLength);

    ssize_t SendResult = send(NewFd, ModbusFrame, MODBUS_TCP_HEADER_LEN + Length, 0);

    return SendResult;
}


void
RunModbusTestServer(sdb_barrier *Barrier)
{
    SdbLogInfo("Running Modbus Test Server");

    srand(time(NULL));

    int                SockFd, NewFd;
    struct sockaddr_in ServerAddr, ClientAddr;
    socklen_t          SinSize;
    char               ClientIp[INET6_ADDRSTRLEN];
    int                Port = MODBUS_PORT;

    SockFd = socket(AF_INET, SOCK_STREAM, 0);
    if(SockFd == -1) {
        SdbLogError("Failed to create socket: %s (errno: %d)", strerror(errno), errno);
        return;
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
        return;
    }

    if(listen(SockFd, BACKLOG) == -1) {
        SdbLogError("Failed to listen on socket: %s (errno: %d)", strerror(errno), errno);
        close(SockFd);
        return;
    }

    SdbLogDebug("Server: waiting for connections on port %d...", Port);

    SinSize = sizeof(ClientAddr);
    NewFd   = accept(SockFd, (struct sockaddr *)&ClientAddr, &SinSize);
    if(NewFd == -1) {
        SdbLogError("Error accepting connection: %s (errno: %d)", strerror(errno), errno);
        close(SockFd);
        return;
    }

    inet_ntop(ClientAddr.sin_family, &(ClientAddr.sin_addr), ClientIp, sizeof(ClientIp));
    SdbLogInfo("Server: accepted connection from %s:%d", ClientIp, ntohs(ClientAddr.sin_port));

    SdbBarrierWait(Barrier);

    for(u64 i = 0; i < MODBUS_PACKET_COUNT; ++i) {
        if(SendModbusData(NewFd) == -1) {
            SdbLogError("Failed to send Modbus data to client %s:%d, closing connection", ClientIp,
                        ntohs(ClientAddr.sin_port));
            break;
        } else {
            if((i % (u64)(MODBUS_PACKET_COUNT / 5)) == 0) {
                SdbLogDebug("Successfully sent Modbus data to client %s:%d", ClientIp,
                            ntohs(ClientAddr.sin_port));
            }
        }
        // SdbSleep(SDB_TIME_S(1.0 / PACKET_FREQ));
    }

    SdbLogDebug("All data sent, stopping server");
    close(NewFd);
    close(SockFd);

    return;
}
