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

#define MODBUS_TCP_HEADER_LEN 7
#define MAX_MODBUS_PDU_SIZE   253
#define MAX_MODBUS_TCP_FRAME  260
#define BACKLOG               5
#define PACKET_FREQ           10


#define READ_HOLDING_REGISTERS 0x03

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
    u8 ModbusFrame[MODBUS_TCP_HEADER_LEN + MAX_MODBUS_PDU_SIZE];

    u16 TransactionId = 1;
    u16 ProtocolId    = 0;
    u8  FunctionCode  = READ_HOLDING_REGISTERS;

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
RunModbusTestServer(sdb_thread *Thread)
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
    close(SockFd);
    return 0;
}


sdb_errno
MbInitTest(comm_protocol_api *Mb)
{
    MbThreadArenasInit();
    SdbThreadArenasInitExtern(Modbus);
    sdb_arena *Scratch1 = SdbArenaBootstrap(&Mb->Arena, NULL, SdbKibiByte(512));
    sdb_arena *Scratch2 = SdbArenaBootstrap(&Mb->Arena, NULL, SdbKibiByte(512));
    SdbThreadArenasAdd(Scratch1);
    SdbThreadArenasAdd(Scratch2);

    modbus_ctx *MbCtx     = SdbPushStruct(&Mb->Arena, modbus_ctx);
    MbCtx->Threads        = SdbPushArray(&Mb->Arena, sdb_thread, Mb->SensorCount);
    MbCtx->ThreadControls = SdbPushArray(&Mb->Arena, sdb_thread_control, Mb->SensorCount);
    MbCtx->ThreadContexts = SdbPushArray(&Mb->Arena, mb_thread_ctx *, Mb->SensorCount);
    Mb->Ctx               = MbCtx;

    u64 InitializedCount = 0;
    for(u64 t = 0; t < Mb->SensorCount; ++t) {
        if(SdbTCtlInit(&MbCtx->ThreadControls[t]) != 0) {
            // Cleanup previously initialized controls
            for(u64 i = 0; i < InitializedCount; ++i) {
                SdbTCtlDeinit(&MbCtx->ThreadControls[i]);
            }
            return -1;
        }
        InitializedCount++;
    }

    if(MbPrepareThreads(Mb) != 0) {
        return -1;
    }

    for(u64 t = 0; t < Mb->SensorCount; ++t) {
        sdb_thread    *Thread    = &MbCtx->Threads[t];
        mb_thread_ctx *ThreadCtx = MbCtx->ThreadContexts[t];

        sdb_errno TRet = SdbThreadCreate(Thread, MbSensorThread, ThreadCtx);
        if(TRet != 0) {
            SdbLogError("Failed to create Postgres main thread for sensor idx %lu", t);
            return TRet;
        }
    }

    return 0;
}

sdb_errno
MbRunTest(comm_protocol_api *Mb)
{
    return 0;
}

sdb_errno
MbFinalizeTest(comm_protocol_api *Mb)
{
    modbus_ctx *MbCtx = MB_CTX(Mb);
    sdb_errno   Ret   = 0;

    // Signal all threads to stop first
    for(u64 t = 0; t < Mb->SensorCount; ++t) {
        SdbTCtlSignalStop(&MbCtx->ThreadControls[t]);
    }
    /*
    // Close all sockets
    for(u64 t = 0; t < Mb->SensorCount; ++t) {
        if (MbCtx->ThreadContexts[t]->SockFd != -1) {
            close(MbCtx->ThreadContexts[t]->SockFd);
            MbCtx->ThreadContexts[t]->SockFd = -1;
        }
    }
    */
    // Wait for threads to stop and cleanup
    for(u64 t = 0; t < Mb->SensorCount; ++t) {
        sdb_errno CtlRet = SdbTCtlWaitForStop(&MbCtx->ThreadControls[t]);
        if(CtlRet != 0) {
            SdbLogError("Thread %lu failed to stop", t);
            Ret = CtlRet;
        }

        sdb_errno TRet = SdbThreadJoin(&MbCtx->Threads[t]);
        if(TRet != 0) {
            SdbLogError("Thread %lu failed to join", t);
            Ret = Ret ? Ret : TRet;
        }

        // Cleanup thread control
        sdb_errno DeinitRet = SdbTCtlDeinit(&MbCtx->ThreadControls[t]);
        if(DeinitRet != 0) {
            SdbLogError("Failed to deinit thread control %lu", t);
            Ret = Ret ? Ret : DeinitRet;
        }
    }

    return Ret;
}
