#include <signal.h>
#include <stdlib.h>
#include <string.h>

#define SDB_H_IMPLEMENTATION
#include <src/Sdb.h>
#undef SDB_H_IMPLEMENTATION

SDB_LOG_REGISTER(Main);

#include <src/CommProtocols/CommProtocols.h>
#include <src/Common/SensorDataPipe.h>
#include <src/Common/Thread.h>
#include <src/Common/Time.h>
#include <src/DatabaseSystems/DatabaseInitializer.h>
#include <src/DatabaseSystems/DatabaseSystems.h>
#include <src/Libs/cJSON/cJSON.h>

#include <src/DevUtils/ModbusMockApi.h>
#include <src/DevUtils/ModbusTestServer.h>

#define SD_PIPE_BUF_COUNT 4

static volatile sig_atomic_t GlobalShutdown = 0;
static sdb_mutex             ShutdownMutex;
static sdb_cond              ShutdownCond;
static sdb_thread            SignalHandlerThread;


sdb_errno
SignalHandler(sdb_thread *Thread)
{
    sigset_t *SigSet = Thread->Args;
    int       Signal;

    while(!GlobalShutdown) {
        if(sigwait(SigSet, &Signal) == 0) {
            switch(Signal) {
                case SIGINT:
                case SIGTERM:
                    SdbLogInfo("Received shutdown signal %s", strsignal(Signal));
                    GlobalShutdown = 1;
                    SdbCondSignal(&ShutdownCond);
                    break;
                default:
                    SdbLogInfo("Received unhandled signal %d %s", Signal, strsignal(Signal));
                    break;
            }
        }
    }

    return 0;
}

int
main(int ArgCount, char **ArgV)
{
    // TODO(ingar): It might be more prudent for the api's to malloc memory directly if main isn't
    // going to use any anyways
    sdb_arena SdbArena;
    u64       SdbArenaSize = SdbMebiByte(128);
    u8       *SdbArenaMem  = malloc(SdbArenaSize);
    if(NULL == SdbArenaMem) {
        SdbLogError("Failed to allocate memory for arena");
        exit(EXIT_FAILURE);
    } else {
        SdbArenaInit(&SdbArena, SdbArenaMem, SdbArenaSize);
    }

    u64               SensorCount = 0;
    sdb_scratch_arena Scratch     = SdbScratchBegin(&SdbArena);
    cJSON *SchemaConf = DbInitGetConfFromFile("./configs/sensor_schemas.json", Scratch.Arena);
    cJSON *SensorSchemaArray = cJSON_GetObjectItem(SchemaConf, "sensors");
    if(SensorSchemaArray == NULL || !cJSON_IsArray(SensorSchemaArray)) {
        SdbLogError("Schema JSON file is malformed");
        exit(EXIT_FAILURE);
    } else {
        SensorCount = cJSON_GetArraySize(SensorSchemaArray);
        cJSON_Delete(SchemaConf);
        SdbScratchRelease(Scratch);
    }


    sensor_data_pipe **SdPipes
        = SdPipesInit(SensorCount, SD_PIPE_BUF_COUNT, SdbKibiByte(32), &SdbArena);
    if(SdPipes == NULL) {
        SdbLogError("Failed to init sensor data pipes");
        exit(EXIT_FAILURE);
    }


    // const u32   ThreadCount = 3;
    const u32   ThreadCount = 2;
    sdb_barrier ModulesBarrier;
    SdbBarrierInit(&ModulesBarrier, ThreadCount);
    // NOTE(ingar): This is used to ensure that all modules have been initialized before
    // starting their main loop


    db_module_ctx *DbmCtx = DbModuleInit(&ModulesBarrier, Dbs_Postgres, DbsApiInit, SdPipes,
                                         SensorCount, SdbMebiByte(9), SdbMebiByte(8), &SdbArena);

    // comm_module_ctx *CommCtx
    //     = CommModulePrepare(&ModulesBarrier, Comm_Protocol_Modbus_TCP, CpApiInit, SdPipes,
    //                         SensorCount, SdbMebiByte(9), SdbMebiByte(8), &SdbArena);

    // TODO(ingar): For testing, remove
    comm_module_ctx *CommCtx
        = CommModulePrepare(&ModulesBarrier, Comm_Protocol_Modbus_TCP, MbMockApiInit, SdPipes,
                            SensorCount, SdbMebiByte(9), SdbMebiByte(8), &SdbArena);

    sigset_t SigSet;
    sigemptyset(&SigSet);
    sigaddset(&SigSet, SIGINT);
    sigaddset(&SigSet, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &SigSet, NULL);
    SdbThreadCreate(&SignalHandlerThread, SignalHandler, &SigSet);


    sdb_thread DbmThread, CommThread, ModbusServerThread;
    // SdbThreadCreate(&ModbusServerThread, RunModbusTestServer, &ModulesBarrier);
    SdbThreadCreate(&DbmThread, DbModuleRun, DbmCtx);
    SdbThreadCreate(&CommThread, CommModuleRun, CommCtx);


    SdbMutexInit(&ShutdownMutex);
    SdbCondInit(&ShutdownCond);

    // TODO(ingar): Find some way for the modules to initiate a shutdown (for development, prod will
    // need some other recovery mechanism) if they fail
    // TODO(ingar): Probably make global shutdown externally available
    SdbMutexLock(&ShutdownMutex, SDB_TIMEOUT_MAX);
    while(!GlobalShutdown) {
        SdbCondWait(&ShutdownCond, &ShutdownMutex, SDB_TIMEOUT_MAX);
    }
    SdbMutexUnlock(&ShutdownMutex);

    SdbTCtlSignalStop(&DbmCtx->Control);
    SdbTCtlSignalStop(&CommCtx->Control);

    SdbTCtlWaitForStop(&DbmCtx->Control);
    SdbTCtlWaitForStop(&CommCtx->Control);

    sdb_errno ModbusRet = 0; // SdbThreadJoin(&ModbusServerThread);
    sdb_errno DbmRet    = SdbThreadJoin(&DbmThread);
    sdb_errno CommRet   = SdbThreadJoin(&CommThread);

    if(DbmRet == 0 && CommRet == 0 && ModbusRet == 0) {
        SdbLogInfo("All threads finished with success!");
        exit(EXIT_SUCCESS);
    } else {
        SdbLogError("Threads finished with errors!");
        exit(EXIT_FAILURE);
    }
}
