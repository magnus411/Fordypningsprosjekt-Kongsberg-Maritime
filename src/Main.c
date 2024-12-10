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
#include <src/Common/ThreadGroup.h>
#include <src/Common/Time.h>
#include <src/CpDbCouplings/CpDbCouplings.h>
#include <src/DatabaseSystems/DatabaseInitializer.h>
#include <src/DatabaseSystems/DatabaseSystems.h>

#include <src/Libs/cJSON/cJSON.h>

#include <src/DevUtils/ModbusMockApi.h>
#include <src/DevUtils/ModbusTestServer.h>

static volatile sig_atomic_t GlobalShutdown = 0;
static sdb_mutex             ShutdownMutex;
static sdb_cond              ShutdownCond;
static pthread_t             SignalHandlerThread;

sdb_errno
SetUpFromConf(sdb_string ConfFilename, tg_manager **Manager)
{
    sdb_errno Ret = 0;

    sdb_file_data *ConfFile = SdbLoadFileIntoMemory(ConfFilename, NULL);
    if(ConfFile == NULL) {
        return -ENOMEM;
    }

    cJSON *Conf = cJSON_Parse((char *)ConfFile->Data);
    if(Conf == NULL) {
        Ret = -SDBE_JSON_ERR;
        goto cleanup;
    }


    cJSON *CpDbCouplingConfs = cJSON_GetObjectItem(Conf, "cp_db_couplings");
    if(CpDbCouplingConfs == NULL || !cJSON_IsArray(CpDbCouplingConfs)) {
        Ret = -SDBE_JSON_ERR;
        goto cleanup;
    }

    u64 CouplingCount = cJSON_GetArraySize(CpDbCouplingConfs);
    if(CouplingCount <= 0) {
        SdbLogError("No Cp-Db couplings were found in the configuration file (count was %lu)",
                    CouplingCount);
        Ret = -EINVAL;
        goto cleanup;
    }

    // NOTE(ingar): This *can* be done programatically, but just manually adjusting it is simpler
#define ALL_COUPLINGS_MAX_TASK_COUNT 3
    u64 ManagerSize = sizeof(tg_manager) + CouplingCount * sizeof(tg_group *);
    u64 TgsSize = CouplingCount * sizeof(tg_group) + ALL_COUPLINGS_MAX_TASK_COUNT * sizeof(tg_task)
                + ALL_COUPLINGS_MAX_TASK_COUNT * sizeof(pthread_t);
#undef ALL_COUPLINGS_MAX_TASK_COUNT

    // NOTE(ingar): Using an arena in this manner isn't *exactly* their intended use-case, but I've
    // already written the code to use an arena, so I'm doing this for expedience
    // NOTE(ingar): The memory that the manager and thread groups live in is malloced, so it will
    // exist even if the arena goes out of scope. The thread groups will live for the entirety of
    // the program so freeing isn't an issue
    // NOTE(ingar): All of this is kinda cursed
    sdb_arena TgArena;
    u64       TgArenaSize = ManagerSize + TgsSize;
    u8       *TgAMem      = malloc(TgArenaSize);
    SdbArenaInit(&TgArena, TgAMem, TgArenaSize);
    if(TgAMem == NULL) {
        SdbLogError("Failed to malloc thread group memory");
        Ret = -ENOMEM;
        goto cleanup;
    }

    *Manager = SdbPushStruct(&TgArena, tg_manager);
    Ret      = TgInitManager(*Manager, CouplingCount, &TgArena);
    if(Ret != 0) {
        SdbLogError("Failed to init thread group manager");
        goto cleanup;
    }

    u64    t = 0;
    cJSON *CouplingConf;
    cJSON_ArrayForEach(CouplingConf, CpDbCouplingConfs)
    {
        (*Manager)->Groups[t] = CpDbCouplingCreateTg(CouplingConf, *Manager, t, &TgArena);
        if((*Manager)->Groups[t] == NULL) {
            cJSON *CouplingName = cJSON_GetObjectItem(CouplingConf, "name");
            SdbLogError("Unable to create thread group for Cp-Db coupling %s",
                        cJSON_GetStringValue(CouplingName));
            Ret = -1;
            goto cleanup;
        }
    }

cleanup:
    if(Ret == -SDBE_JSON_ERR) {
        SdbLogError("Error parsing JSON: %s", cJSON_GetErrorPtr());
    }
    if((Ret != 0) && (TgAMem != NULL)) {
        free(TgAMem);
    }

    free(ConfFile);
    cJSON_Delete(Conf);
    return Ret;
}

void *
SignalHandler(void *Arg)
{
    sigset_t *SigSet = Arg;
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
    tg_manager *Manager = NULL;
    SetUpFromConf("./configs/sdb_conf.json", &Manager);
    if(Manager == NULL) {
        SdbLogError("Failed to set up from config file");
        exit(EXIT_FAILURE);
    }

    sigset_t SigSet;
    sigemptyset(&SigSet);
    sigaddset(&SigSet, SIGINT);
    sigaddset(&SigSet, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &SigSet, NULL);
    pthread_create(&SignalHandlerThread, NULL, SignalHandler, &SigSet);
    pthread_detach(SignalHandlerThread);

    sdb_errno TgStartRet = TgManagerStartAll(Manager);
    if(TgStartRet != 0) {
        exit(EXIT_FAILURE);
    }


    SdbMutexInit(&ShutdownMutex);
    SdbCondInit(&ShutdownCond);

    SdbMutexLock(&ShutdownMutex, SDB_TIMEOUT_MAX);
    while(!GlobalShutdown) {
        SdbCondWait(&ShutdownCond, &ShutdownMutex, SDB_TIMEOUT_MAX);
    }
    SdbMutexUnlock(&ShutdownMutex);
}
