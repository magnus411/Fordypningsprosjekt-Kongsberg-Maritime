#include "src/Common/SensorDataPipe.h"
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <src/Sdb.h>

SDB_LOG_REGISTER(TestModbus);
SDB_THREAD_ARENAS_EXTERN(Modbus);

#include <src/CommProtocols/Modbus.h>
#include <src/Common/CircularBuffer.h>
#include <src/Common/Socket.h>
#include <src/Common/Thread.h>
#include <src/Common/Time.h>
#include <src/DatabaseSystems/Postgres.h>
#include <tests/TestConstants.h>

#define BACKLOG     5
#define PACKET_FREQ 10

typedef struct __attribute__((packed, aligned(1)))
{
    pg_int8   PacketId;
    pg_float8 Time;
    pg_float8 Rpm;
    pg_float8 Torque;
    pg_float8 Power;
    pg_float8 PeakPeakPfs;
} shaft_power_data;

static void
GeneratePowerShaftData(shaft_power_data *Data)
{
    time_t CurrentTime = time(NULL);
    localtime(&CurrentTime);
    static i64 Id = 0;
    if(rand() % 10 < 2) {
        Data->PacketId    = Id++;
        Data->Time        = (pg_float8)difftime(CurrentTime, 0);
        Data->Rpm         = 0;
        Data->Torque      = 0;
        Data->Power       = 0;
        Data->PeakPeakPfs = 0;
    } else {
        Data->PacketId    = Id++;
        Data->Time        = (pg_float8)difftime(CurrentTime, 0);
        Data->Rpm         = ((rand() % 100) + 30.0) * sin(Id * 0.1); // RPM with variation
        Data->Torque      = ((rand() % 600) + 100.0) * 0.8;          // Torque (Nm)
        Data->Power       = Data->Rpm * Data->Torque / 9.5488;       // Derived power (kW)
        Data->PeakPeakPfs = ((rand() % 50) + 25.0) * cos(Id * 0.1);  // PFS variation
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
SendModbusData(int NewFd, u16 UnitId)
{
    u8 ModbusFrame[MODBUS_TCP_FRAME_MAX_SIZE];

    u16 TransactionId = 1;
    u16 ProtocolId    = 0;
    u8  FunctionCode  = MODBUS_READ_HOLDING_REGISTERS;

    shaft_power_data Data = { 0 };

    u16 DataLength = sizeof(Data);
    u16 Length     = DataLength + 3;

    GeneratePowerShaftData(&Data);
    GenerateModbusTcpFrame(ModbusFrame, TransactionId, ProtocolId, Length, UnitId, FunctionCode,
                           (u8 *)&Data, DataLength);

    ssize_t SendResult = send(NewFd, ModbusFrame, MODBUS_TCP_HEADER_LEN + Length, 0);

    return SendResult;
}


sdb_errno
ModbusTestRunServer(sdb_thread *Thread)
{
    SdbLogInfo("Running Modbus Test Server");

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

        SdbSleep(SDB_TIME_S(1.0 / PACKET_FREQ));
    }
    SdbLogDebug("All data sent, stopping server");
    close(NewFd);
    close(SockFd);
    return 0;
}


sdb_errno
MbTestApiInit(comm_protocol_api *Mb)
{
    MbThreadArenasInit();
    SdbThreadArenasInitExtern(Modbus);
    sdb_arena *Scratch1 = SdbArenaBootstrap(&Mb->Arena, NULL, SdbKibiByte(512));
    sdb_arena *Scratch2 = SdbArenaBootstrap(&Mb->Arena, NULL, SdbKibiByte(512));
    SdbThreadArenasAdd(Scratch1);
    SdbThreadArenasAdd(Scratch2);

    Mb->Ctx = MbPrepareCtx(Mb);
    if(Mb->Ctx == NULL) {
        SdbLogError("Failed to initialize Modbus context");
        return -1;
    }

    return 0;
}

sdb_errno
MbTestApiRun(comm_protocol_api *Mb)
{
    sdb_errno Ret = 0;

    modbus_ctx         *MbCtx         = MB_CTX(Mb);
    sdb_thread_control *ModuleControl = Mb->ModuleControl;
    sdb_arena          *MbArena       = &Mb->Arena;
    mb_conn             Conn = MbCtx->Conns[0]; // TODO(ingar): Simplified to only use one for now

    sensor_data_pipe *Pipe = Mb->SdPipes[0]; // TODO(ingar): Simplified to only use one pipe for now
    sdb_arena        *CurBuf = Pipe->Buffers[atomic_load(&Pipe->WriteBufIdx)];

    while(!SdbTCtlShouldStop(ModuleControl)) {
        SdbAssert((SdbArenaGetPos(CurBuf) <= Pipe->BufferMaxFill),
                  "Thread %ld: Pipe buffer overflow in buffer %u", Thread->pid,
                  atomic_load(&Pipe->WriteBufIdx));

        if(SdbArenaGetPos(CurBuf) == Pipe->BufferMaxFill) {
            CurBuf = SdPipeGetWriteBuffer(Pipe);
        }

        u8  Frame[MODBUS_TCP_FRAME_MAX_SIZE];
        int RecvResult = SocketRecvWithTimeout(Conn.SockFd, Frame, sizeof(Frame), SDB_TIME_MS(500));

        switch(RecvResult) {
            case -2: // Timeout
                continue;
            case -1: // Error
                Ret = -1;
                goto exit;
                break;
            case 0: // Server disconnected
                Ret = 0;
                goto exit;
                break;
            default: // Data received
                {
                    u16       UnitId, FunctionCode, DataLength;
                    const u8 *Data = MbParseTcpFrame(Frame, &UnitId, &FunctionCode, &DataLength);

                    SdbAssert(DataLength == Pipe->PacketSize,
                              "Modbus packet size did not match expected, was %u expected %zd",
                              DataLength, Pipe->PacketSize);

                    u8 *Ptr = SdbArenaPush(CurBuf, DataLength);
                    SdbMemcpy(Ptr, Data, DataLength);
                }
                break;
        }
    }

exit:
    SdPipeFlush(Pipe);
    SdbLogDebug("Modbus main loop stopped with %s", (Ret == 0) ? "success" : "error");

    return Ret;
}

sdb_errno
MbTestApiFinalize(comm_protocol_api *Mb)
{
    sdb_errno   Ret   = 0;
    modbus_ctx *MbCtx = MB_CTX(Mb);

    for(u64 i = 0; i < MbCtx->ConnCount; ++i) {
        int SockFd = MbCtx->Conns[i].SockFd;
        SdbLogDebug("Closing connection %d", SockFd);
        close(SockFd);
    }

    return Ret;
}
