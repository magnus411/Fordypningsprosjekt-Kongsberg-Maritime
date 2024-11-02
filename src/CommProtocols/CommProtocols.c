#include <errno.h>
#include <pthread.h>

#include <src/Sdb.h>
SDB_LOG_REGISTER(CommProtocols);

#include <src/CommProtocols/CommProtocols.h>
#include <src/CommProtocols/MQTT.h>
#include <src/CommProtocols/Modbus.h>
#include <src/CommProtocols/ModbusApi.h>
#include <src/Common/SensorDataPipe.h>
#include <src/Common/Thread.h>
#include <src/Common/Time.h>

#if COMM_PROTOCOL_MODBUS == 1
static bool ModbusAvailable_ = true;
#else
static bool ModbusAvailable_ = false;
#endif

#if COMM_PROTOCOL_MQTT == 1
static bool MQTTAvailable_ = true;
#else
static bool MQTTAvailable_ = false;
#endif

// TODO(ingar): Put this and the module into the same file (same with database module)?
bool
CpProtocolIsAvailable(Comm_Protocol_Type Type)
{
    switch(Type) {
        case Comm_Protocol_Modbus_TCP:
            return ModbusAvailable_;
        case Comm_Protocol_MQTT:
            return MQTTAvailable_;
        default:
            SdbAssert(0, "Invalid protocol type");
            return false;
    }
}

comm_module_ctx *
CommModulePrepare(sdb_barrier *ModulesBarrier, Comm_Protocol_Type Type, cp_init_api ApiInit,
                  sensor_data_pipe **Pipes, u64 SensorCount, u64 ModuleArenaSize, u64 CpArenaSize,
                  sdb_arena *Arena)
{
    comm_module_ctx *CommCtx = SdbPushStruct(Arena, comm_module_ctx);

    CommCtx->ModulesBarrier = ModulesBarrier;
    CommCtx->CpType         = Type;
    CommCtx->InitApi        = ApiInit;
    CommCtx->SensorCount    = SensorCount;
    CommCtx->SdPipes        = Pipes;
    CommCtx->CpArenaSize    = CpArenaSize;

    SdbArenaBootstrap(Arena, &CommCtx->Arena, ModuleArenaSize);
    SdbTCtlInit(&CommCtx->Control);

    return CommCtx;
}

sdb_errno
CpApiInit(Comm_Protocol_Type Type, sdb_thread_control *ModuleControl, u64 SensorCount,
          sensor_data_pipe **SdPipes, sdb_arena *Arena, u64 ArenaSize, i64 CommTId,
          comm_protocol_api *CpApi)
{
    if(!CpProtocolIsAvailable(Type)) {
        SdbLogWarning("%s is unavailable", CpTypeToName(Type));
        return -SDBE_CP_UNAVAIL;
    }

    SdbMemset(CpApi, 0, sizeof(*CpApi));
    CpApi->ModuleControl = ModuleControl;
    CpApi->SensorCount   = SensorCount;
    CpApi->SdPipes       = SdPipes;
    SdbArenaBootstrap(Arena, &CpApi->Arena, ArenaSize);

    switch(Type) {
        case Comm_Protocol_Modbus_TCP:
            {
                CpApi->Init     = MbInit;
                CpApi->Run      = MbRun;
                CpApi->Finalize = MbFinalize;

                mb_init_args *Args = SdbPushStruct(&CpApi->Arena, mb_init_args);

                Args->IpCount = 1;
                Args->Ips     = SdbPushArray(&CpApi->Arena, sdb_string, 1);
                Args->Ips[0]  = SdbStringMake(&CpApi->Arena, "127.0.0.1");

                Args->PortCount = 1;
                Args->Ports     = SdbPushArray(&CpApi->Arena, int, 1);
                Args->Ports[0]  = MODBUS_PORT;

                CpApi->OptArgs = Args;
            }
            break;
        case Comm_Protocol_MQTT:
            {
                CpApi->Init     = MqttInit;
                CpApi->Run      = MqttRun;
                CpApi->Finalize = MqttFinalize;
                CpApi->OptArgs  = (void *)CommTId;
            }
            break;
        default:
            SdbAssert(0, "Comm protocol does not exist");
            return -EINVAL;
    }

    return 0;
}


#define CP_INIT_ATTEMPT_THRESHOLD (5)

sdb_errno
CommModuleRun(sdb_thread *Thread)
{
    comm_module_ctx  *CommCtx = Thread->Args;
    sdb_errno         Ret     = 0;
    comm_protocol_api ThreadCp;

    if((Ret = CommCtx->InitApi(CommCtx->CpType, &CommCtx->Control, CommCtx->SensorCount,
                               CommCtx->SdPipes, &CommCtx->Arena, CommCtx->CpArenaSize, Thread->pid,
                               &ThreadCp))
       == -SDBE_CP_UNAVAIL) {
        SdbLogWarning("Thread %ld: Attempting to use %s, but its API is unavailable", Thread->pid,
                      CpTypeToName(CommCtx->CpType));
        goto exit;
    }


    i64 Attempts = 0; // TODO(ingar): Is this necessary?
    while(((Ret = ThreadCp.Init(&ThreadCp)) != 0) && Attempts++ < CP_INIT_ATTEMPT_THRESHOLD) {
        SdbLogError("Thread %ld: Error on comm protocol init attempt %ld, ret: %d", Thread->pid,
                    Attempts, Ret);
    }

    // NOTE(ingar): Wait for all modules to be initialized. This has to be before the threshold
    // check, otherwise the other modules will be left waiting at the barrier, hardlocking the
    // program
    SdbBarrierWait(CommCtx->ModulesBarrier);
    // TODO(ingar): Interrupt to stop program doesn't work when module fails to connect to the
    // server

    if(Attempts >= CP_INIT_ATTEMPT_THRESHOLD) {
        SdbLogError(
            "Thread %ld: Comm protocol init attempt threshold exceeded. Marking thread as stopped.",
            Thread->pid);
        goto exit;
    } else {
        SdbLogInfo("Thread %ld: Comm protocol successfully initialized. Starting main loop",
                   Thread->pid);
    }


    if((Ret = ThreadCp.Run(&ThreadCp)) >= 0) {
        SdbLogInfo("Thread %ld: Comm protocol loop exited with success. Starting finalization",
                   Thread->pid);
    } else {
        SdbLogError("Thread %ld: Comm protocol loop exited with error: %d", Thread->pid, Ret);
        goto exit;
    }


    if((Ret = ThreadCp.Finalize(&ThreadCp)) == 0) {
        SdbLogInfo("Thread %ld: Comm protocol successfully finalized", Thread->pid);
    } else {
        SdbLogError("Thread %ld: Comm protocol finalization failed", Thread->pid);
    }


exit:
    SdbTCtlMarkStopped(&CommCtx->Control);
    return Ret;
}
