#include <src/Sdb.h>
SDB_LOG_REGISTER(DbModule);

#include <src/Common/CircularBuffer.h>
#include <src/Common/Errno.h>
#include <src/Common/Thread.h>
#include <src/DatabaseSystems/DatabaseSystems.h>
#include <src/Modules/DatabaseModule.h>

#define DB_INIT_ATTEMPT_THRESHOLD (5)

sdb_errno
DbModuleRun(sdb_thread *Thread)
{
    db_module_ctx *DbmCtx  = Thread->Args;
    sdb_errno      Ret     = 0;
    void          *OptArgs = NULL;
    database_api   ThreadDb;


    if((Ret = DbsInitApi(DbmCtx->DbsType, &DbmCtx->SdPipe, &DbmCtx->Arena, DbmCtx->DbsArenaSize,
                         Thread->pid, &ThreadDb, OptArgs))
       == -SDBE_DBS_UNAVAIL) {
        SdbLogWarning("Thread %ld: Attempting to run %s, but its API is unavailable", Thread->pid,
                      DbsTypeToName(DbmCtx->DbsType));
        goto exit;
    }


    i64 Attempts = 0;
    while(((Ret = ThreadDb.Init(&ThreadDb, OptArgs)) != 0)
          && (Attempts++ < DB_INIT_ATTEMPT_THRESHOLD)) {
        SdbLogError("Thread %ld: Error on database init attempt %ld, ret: %d", Thread->pid,
                    Attempts, Ret);
    }

    SdbBarrierWait(DbmCtx->ModulesBarrier);

    if(Attempts >= DB_INIT_ATTEMPT_THRESHOLD) {
        SdbLogError("Thread %ld: Database init attempt threshold exceeded", Thread->pid);
        goto exit;
    } else {
        SdbLogInfo("Thread %ld: Database successfully initialized", Thread->pid);
    }


    if((Ret = ThreadDb.Run(&ThreadDb)) == 0) {
        SdbLogInfo("Thread %ld: Database has stopped and returned success", Thread->pid);
    } else {
        SdbLogError("Thread %ld: Database has stopped and returned an error: %d", Thread->pid, Ret);
        goto exit;
    }


    if((Ret = ThreadDb.Finalize(&ThreadDb)) == 0) {
        SdbLogInfo("Thread %ld: Database successfully finalized", Thread->pid);
    } else {
        SdbLogError("Thread %ld: Database finilization failed", Thread->pid);
    }

exit:
    return Ret;
}
