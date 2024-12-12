#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <src/Sdb.h>

SDB_LOG_REGISTER(ModbusTestServer);

#include <netinet/tcp.h>
#include <src/CommProtocols/Modbus.h>
#include <src/Common/CircularBuffer.h>
#include <src/Common/Socket.h>
#include <src/Common/Thread.h>
#include <src/Common/Time.h>
#include <src/DatabaseSystems/Postgres.h>
#include <src/DevUtils/TestConstants.h>
#include <src/Signals.h>

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
    static u8        ModbusFrame[MODBUS_TCP_FRAME_MAX_SIZE] = { 1 };
    static const u16 DataLength                             = sizeof(shaft_power_data);
    static const u16 Length                                 = DataLength + 3;
    shaft_power_data SpData;
    static u64       sendCount = 0;

    // Generate random data
    GenerateShaftPowerDataRandom(&SpData);

    u16 TransactionId = 1;
    u16 ProtocolId    = 1;
    u8  FunctionCode  = 1;

    ModbusFrame[8] = DataLength;
    SdbMemcpy(&ModbusFrame[9], &SpData, DataLength);

    ssize_t SendResult = send(NewFd, ModbusFrame, MODBUS_TCP_HEADER_LEN + Length, 0);

    if(SendResult > 0) {
        sendCount++;
        if(sendCount % 1000000 == 0) {
            SdbLogInfo("Sent %lu packets", sendCount);
        }
    }

    return SendResult;
}

void
RunModbusTestServer(sdb_barrier *Barrier)
{
    SdbLogInfo("Running Modbus Test Server");

    srand(time(NULL));

    int SockFd = socket(AF_INET, SOCK_STREAM, 0);
    if(SockFd == -1) {
        SdbLogError("Failed to create socket: %s (errno: %d)", strerror(errno), errno);
        return;
    }

    // Add socket options
    int OptVal = 1;
    if(setsockopt(SockFd, SOL_SOCKET, SO_REUSEADDR, &OptVal, sizeof(OptVal)) == -1) {
        SdbLogError("Failed to set SO_REUSEADDR: %s", strerror(errno));
        close(SockFd);
        return;
    }

    // Add TCP_NODELAY to prevent buffering
    if(setsockopt(SockFd, IPPROTO_TCP, TCP_NODELAY, &OptVal, sizeof(OptVal)) == -1) {
        SdbLogError("Failed to set TCP_NODELAY: %s", strerror(errno));
        close(SockFd);
        return;
    }

    struct sockaddr_in ServerAddr;
    ServerAddr.sin_family      = AF_INET;
    ServerAddr.sin_port        = htons(MODBUS_PORT);
    ServerAddr.sin_addr.s_addr = INADDR_ANY;
    SdbMemset(&(ServerAddr.sin_zero), '\0', 8);

    if(bind(SockFd, (struct sockaddr *)&ServerAddr, sizeof(ServerAddr)) == -1) {
        SdbLogError("Failed to bind: %s (errno: %d)", strerror(errno), errno);
        close(SockFd);
        return;
    }

    if(listen(SockFd, BACKLOG) == -1) {
        SdbLogError("Failed to listen: %s (errno: %d)", strerror(errno), errno);
        close(SockFd);
        return;
    }

    // Make socket non-blocking
    int flags = fcntl(SockFd, F_GETFL, 0);
    fcntl(SockFd, F_SETFL, flags | O_NONBLOCK);

    SdbLogInfo("Server: waiting for connections on port %d...", MODBUS_PORT);
    SdbBarrierWait(Barrier);

    while(!SdbShouldShutdown()) {
        struct sockaddr_in ClientAddr;
        socklen_t          SinSize = sizeof(ClientAddr);
        char               ClientIp[INET6_ADDRSTRLEN];

        int NewFd = accept(SockFd, (struct sockaddr *)&ClientAddr, &SinSize);
        if(NewFd == -1) {
            if(errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(10000); // 10ms delay if no connection
                continue;
            }
            SdbLogError("Accept error: %s (errno: %d)", strerror(errno), errno);
            continue;
        }

        // Set client socket to non-blocking too
        flags = fcntl(NewFd, F_GETFL, 0);
        fcntl(NewFd, F_SETFL, flags | O_NONBLOCK);

        // Enable TCP_NODELAY for client socket
        OptVal = 1;
        setsockopt(NewFd, IPPROTO_TCP, TCP_NODELAY, &OptVal, sizeof(OptVal));

        inet_ntop(ClientAddr.sin_family, &(ClientAddr.sin_addr), ClientIp, sizeof(ClientIp));
        SdbLogInfo("Server: accepted connection from %s:%d", ClientIp, ntohs(ClientAddr.sin_port));

        u64             packetsSent = 0;
        struct timespec lastSend, now;
        clock_gettime(CLOCK_MONOTONIC, &lastSend);

        while(!SdbShouldShutdown()) {
            clock_gettime(CLOCK_MONOTONIC, &now);

            // Control send rate
            if((now.tv_sec - lastSend.tv_sec) * 1000000 + (now.tv_nsec - lastSend.tv_nsec) / 1000
               < 100) { // 100 microseconds = 10kHz
                continue;
            }

            sdb_errno sendResult = SendModbusData(NewFd);
            if(sendResult == -1) {
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                    usleep(1000); // Wait if buffer is full
                    continue;
                }
                SdbLogError("Failed to send to %s:%d: %s", ClientIp, ntohs(ClientAddr.sin_port),
                            strerror(errno));
                break;
            }


            lastSend = now;
        }

        SdbLogInfo("Connection closed after sending %lu packets", packetsSent);
        close(NewFd);

        if(SdbShouldShutdown()) {
            break;
        }
    }

    SdbLogInfo("Server shutting down");
    close(SockFd);
}