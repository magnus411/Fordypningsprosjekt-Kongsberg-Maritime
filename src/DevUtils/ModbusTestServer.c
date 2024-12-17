

/**
 * @file ModbusTestServer.c
 * @brief Implementation of Modbus Test Server
 *
 * Provides an implementation of a Modbus TCP test server
 * that generates and transmits simulated shaft power data.
 *
 * Main Responsibilities:
 * - Create and manage TCP server socket
 * - Generate random shaft power measurement data
 * - Handle client connections
 * - Implement controlled data transmission
 *
 * Key Components:
 * - Non-blocking socket operations
 * - Random data generation
 * - Rate-limited data transmission
 *
 */

#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <src/Sdb.h>

SDB_LOG_REGISTER(ModbusTestServer);

#include <netinet/tcp.h>
#include <src/CommProtocols/Modbus.h>
#include <src/Common/Socket.h>
#include <src/Common/Thread.h>
#include <src/Common/Time.h>
#include <src/DatabaseSystems/Postgres.h>
#include <src/DevUtils/TestConstants.h>
#include <src/Signals.h>

#define BACKLOG     5
#define PACKET_FREQ 1e5


/**
 * @brief Generates deterministic shaft power data
 *
 * Creates a shaft power data structure with incrementing values.
 * Useful for consistent, predictable data generation.
 *
 * @param Data Pointer to shaft_power_data structure to be filled
 */
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


/**
 * @brief Generates random shaft power data
 *
 * Creates a shaft power data structure with randomized values.
 * Simulates realistic variations in power measurements.
 *
 * Generates:
 * - Randomized RPM with sinusoidal variation
 * - Randomized Torque
 * - Calculated Power
 * - Randomized Peak-to-Peak PFS
 *
 * @param Data Pointer to shaft_power_data structure to be filled
 */
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


/**
 * @brief Constructs a Modbus TCP frame
 *
 * Builds a complete Modbus TCP frame with specified parameters,
 * including transaction ID, protocol ID, length, unit ID,
 * function code, and payload data.
 *
 * @param Buffer Output buffer to store the constructed frame
 * @param TransactionId Modbus transaction identifier
 * @param ProtocolId Modbus protocol identifier
 * @param Length Total length of the frame payload
 * @param UnitId Modbus unit identifier
 * @param FunctionCode Modbus function code
 * @param Data Payload data to be included in the frame
 * @param DataLength Length of the payload data
 */
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


/**
 * @brief Sends Modbus data over a socket
 *
 * Generates a random shaft power data packet, constructs a
 * Modbus TCP frame, and sends it over the provided socket.
 *
 * Features:
 * - Random data generation
 * - Modbus TCP frame construction
 * - Periodic logging of sent packets
 *
 * @param NewFd Socket file descriptor to send data
 * @return sdb_errno Status of send operation
 */
static inline sdb_errno
SendModbusData(int NewFd)
{
    static u8        ModbusFrame[MODBUS_TCP_FRAME_MAX_SIZE] = { 0 };
    static const u16 DataLength                             = sizeof(shaft_power_data);
    static const u16 Length                                 = DataLength + 3;
    shaft_power_data SpData;
    static u64       SendCount = 0;

    // Sanity check the sizes
    if(Length > MODBUS_TCP_FRAME_MAX_SIZE - MODBUS_TCP_HEADER_LEN) {
        SdbLogError("Frame too large: %u", Length);
        return -1;
    }

    memset(ModbusFrame, 0, MODBUS_TCP_FRAME_MAX_SIZE);
    GenerateShaftPowerDataRandom(&SpData);

    u16 Pos = 0;
    // Fixed header construction
    ModbusFrame[Pos++] = 0x00;                 // Transaction ID high
    ModbusFrame[Pos++] = 0x01;                 // Transaction ID low
    ModbusFrame[Pos++] = 0x00;                 // Protocol ID high
    ModbusFrame[Pos++] = 0x00;                 // Protocol ID low
    ModbusFrame[Pos++] = (Length >> 8) & 0xFF; // Length high
    ModbusFrame[Pos++] = Length & 0xFF;        // Length low
    ModbusFrame[Pos++] = 0x01;                 // Unit ID
    ModbusFrame[Pos++] = 0x10;                 // Function code
    ModbusFrame[Pos++] = DataLength;           // Byte count

    // Copy data with bounds checking
    if(Pos + DataLength > MODBUS_TCP_FRAME_MAX_SIZE) {
        SdbLogError("Buffer overflow prevented");
        return -1;
    }

    SdbMemcpy(&ModbusFrame[Pos], &SpData, DataLength);
    size_t TotalSize = MODBUS_TCP_HEADER_LEN + Length;

    SdbLogDebug("Sending frame - Length: %u, DataLength: %u, TotalSize: %zu", Length, DataLength,
                TotalSize);

    ssize_t SendResult = send(NewFd, ModbusFrame, TotalSize, 0);
    if(SendResult > 0) {
        SendCount++;
        if(SendCount % 10000 == 0) {
            SdbLogInfo("Sent %lu packets", SendCount);
        }
    }

    return SendResult;
}


/**
 * @brief Main Modbus Test Server routine
 *
 * Implements the complete Modbus test server workflow:
 * - Create and configure server socket
 * - Handle incoming client connections
 * - Send simulated shaft power data
 * - Support non-blocking operations
 * - Graceful shutdown handling
 *
 * @param Barrier Synchronization barrier to coordinate server startup
 */
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
    int Flags = fcntl(SockFd, F_GETFL, 0);
    fcntl(SockFd, F_SETFL, Flags | O_NONBLOCK);

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
        Flags = fcntl(NewFd, F_GETFL, 0);
        fcntl(NewFd, F_SETFL, Flags | O_NONBLOCK);

        // Enable TCP_NODELAY for client socket
        OptVal = 1;
        setsockopt(NewFd, IPPROTO_TCP, TCP_NODELAY, &OptVal, sizeof(OptVal));

        inet_ntop(ClientAddr.sin_family, &(ClientAddr.sin_addr), ClientIp, sizeof(ClientIp));
        SdbLogInfo("Server: accepted connection from %s:%d", ClientIp, ntohs(ClientAddr.sin_port));

        u64             PacketsSent = 0;
        struct timespec LastSend, Now;
        clock_gettime(CLOCK_MONOTONIC, &LastSend);

        while(!SdbShouldShutdown()) {
            clock_gettime(CLOCK_MONOTONIC, &Now);

            // Control send rate
            if((Now.tv_sec - LastSend.tv_sec) * 1000000 + (Now.tv_nsec - LastSend.tv_nsec) / 1000
               < 100) { // 100 microseconds = 10kHz
                continue;
            }

            sdb_errno SendResult = SendModbusData(NewFd);
            if(SendResult == -1) {
                if(errno == EAGAIN || errno == EWOULDBLOCK) {
                    usleep(1000); // Wait if buffer is full
                    continue;
                }
                SdbLogError("Failed to send to %s:%d: %s", ClientIp, ntohs(ClientAddr.sin_port),
                            strerror(errno));
                break;
            }


            LastSend = Now;
        }

        SdbLogInfo("Connection closed after sending %lu packets", PacketsSent);
        close(NewFd);

        if(SdbShouldShutdown()) {
            break;
        }
    }

    SdbLogInfo("Server shutting down");
    close(SockFd);
}
