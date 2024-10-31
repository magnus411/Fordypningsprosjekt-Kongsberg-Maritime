#include <src/Sdb.h>
SDB_LOG_REGISTER(DbSystems);

#include <src/Common/SensorDataPipe.h>
#include <src/Common/Thread.h>
#include <src/Common/Time.h>
#include <src/DatabaseSystems/DatabaseSystems.h>
#include <src/DatabaseSystems/PostgresApi.h>

#if DATABASE_SYSTEM_POSTGRES == 1
static bool PgIsAvailable_ = true;
#else
static bool PgIsAvailable_ = false;
#endif // DATABASE_SYSTEM_POSTGRES

#define DB_INIT_ATTEMPT_THRESHOLD (5)

bool
DbsDatabaseIsAvailable(Db_System_Type DbsId)
{
    switch(DbsId) {
        case Dbs_Postgres:
            return PgIsAvailable_;
        default:
            return false;
    }
}

sdb_errno
DbsApiInit(Db_System_Type Type, sdb_thread_control *ModuleControl, u64 SensorCount,
           sensor_data_pipe **Pipes, sdb_arena *Arena, u64 ArenaSize, i64 DbmTId,
           database_api *DbsApi)
{
    if(!DbsDatabaseIsAvailable(Type)) {
        SdbLogWarning("%s is unavailable", DbsTypeToName(Type));
        return -SDBE_DBS_UNAVAIL;
    }

    SdbMemset(DbsApi, 0, sizeof(*DbsApi));
    DbsApi->ModuleControl = ModuleControl;
    DbsApi->SensorCount   = SensorCount;
    DbsApi->SdPipes       = Pipes;
    SdbArenaBootstrap(Arena, &DbsApi->Arena, ArenaSize);

    switch(Type) {
        case Dbs_Postgres:
            {
                DbsApi->Init     = PgInit;
                DbsApi->Run      = PgRun;
                DbsApi->Finalize = PgFinalize;
            }
            break;
        default:
            {
                SdbAssert(0, "Database type doesn't exist");
                return -EINVAL;
            }
            break;
    }

    return 0;
}

db_module_ctx *
DbModuleInit(sdb_barrier *ModulesBarrier, Db_System_Type Type, dbs_init_api ApiInit,
             sensor_data_pipe **Pipes, u64 SensorCount, u64 ModuleArenaSize, u64 DbsArenaSize,
             sdb_arena *Arena)
{
    db_module_ctx *DbmCtx = SdbPushStruct(Arena, db_module_ctx);

    DbmCtx->ModulesBarrier = ModulesBarrier;
    DbmCtx->DbsType        = Type;
    DbmCtx->InitApi        = ApiInit;
    DbmCtx->SdPipes        = Pipes;
    DbmCtx->SensorCount    = SensorCount;
    DbmCtx->DbsArenaSize   = DbsArenaSize;

    SdbArenaBootstrap(Arena, &DbmCtx->Arena, ModuleArenaSize);
    SdbTCtlInit(&DbmCtx->Control);

    return DbmCtx;
}

sdb_errno
DbModuleRun(sdb_thread *Thread)
{
    db_module_ctx *DbmCtx = Thread->Args;
    sdb_errno      Ret    = 0;
    database_api   ThreadDb;


    if((Ret
        = DbmCtx->InitApi(DbmCtx->DbsType, &DbmCtx->Control, DbmCtx->SensorCount, DbmCtx->SdPipes,
                          &DbmCtx->Arena, DbmCtx->DbsArenaSize, Thread->pid, &ThreadDb))
       == -SDBE_DBS_UNAVAIL) {
        SdbLogWarning("Thread %ld: Attempting to run %s, but its API is unavailable", Thread->pid,
                      DbsTypeToName(DbmCtx->DbsType));
        goto exit;
    }


    i64 Attempts = 0;
    while(((Ret = ThreadDb.Init(&ThreadDb)) != 0) && (Attempts++ < DB_INIT_ATTEMPT_THRESHOLD)) {
        SdbLogError("Thread %ld: Error on database init attempt %ld, ret: %d", Thread->pid,
                    Attempts, Ret);
    }

    if(Attempts >= DB_INIT_ATTEMPT_THRESHOLD) {
        SdbLogError("Thread %ld: Database init attempt threshold exceeded", Thread->pid);
        goto exit;
    } else {
        SdbLogInfo("Thread %ld: Database successfully initialized", Thread->pid);
    }

    SdbBarrierWait(DbmCtx->ModulesBarrier);

    if((Ret = ThreadDb.Run(&ThreadDb)) == 0) {
        SdbLogInfo("Thread %ld: Database has stopped and returned success", Thread->pid);
    } else {
        SdbLogError("Thread %ld: Database has stopped and returned an error: %d", Thread->pid, Ret);
    }

    if((Ret = ThreadDb.Finalize(&ThreadDb)) == 0) {
        SdbLogInfo("Thread %ld: Database successfully finalized", Thread->pid);
    } else {
        SdbLogError("Thread %ld: Database finilization failed", Thread->pid);
    }

exit:
    SdbTCtlMarkStopped(&DbmCtx->Control);
    return Ret;
}
