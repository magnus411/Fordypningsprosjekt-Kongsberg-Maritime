#include <src/Sdb.h>
SDB_LOG_REGISTER(DbModule);

#include <src/Common/CircularBuffer.h>
#include <src/Common/SdbErrno.h>
#include <src/DatabaseSystems/DatabaseSystems.h>
#include <src/Modules/DatabaseModule.h>

#define DB_INIT_ATTEMPT_THRESHOLD (5)

void *
DbModuleRun(void *DbmCtx_)
{
    db_module_ctx *DbmCtx  = DbmCtx_;
    sdb_errno      Ret     = 0;
    void          *OptArgs = NULL;
    database_api   ThreadDb;

    if((Ret = DbsInitApi(DbmCtx->DbsType, &DbmCtx->SdPipe, &DbmCtx->Arena, DbmCtx->DbsArenaSize,
                         DbmCtx->ThreadId, &ThreadDb, OptArgs))
       == -SDBE_DBS_UNAVAIL) {
        SdbLogWarning("Thread %ld: Attempting to run %s, but its API is unavailable",
                      DbmCtx->ThreadId, DbsTypeToName(DbmCtx->DbsType));
        goto exit;
    }

    i64 Attempts = 0;
    while(((Ret = ThreadDb.Init(&ThreadDb, OptArgs)) != 0)
          && (Attempts++ < DB_INIT_ATTEMPT_THRESHOLD)) {
        SdbLogError("Thread %ld: Error on database init attempt %ld, ret: %d", DbmCtx->ThreadId,
                    Attempts, Ret);
    }

    if(Attempts >= DB_INIT_ATTEMPT_THRESHOLD) {
        SdbLogError("Thread %ld: Database init attempt threshold exceeded", DbmCtx->ThreadId);
        goto exit;
    } else {
        SdbLogInfo("Thread %ld: Database successfully initialized", DbmCtx->ThreadId);
    }

    if((Ret = ThreadDb.Run(&ThreadDb)) == 0) {
        SdbLogInfo("Thread %ld: Database has stopped and returned success", DbmCtx->ThreadId);
    } else {
        SdbLogError("Thread %ld: Database has stopped and returned an error: %d", DbmCtx->ThreadId,
                    Ret);
        goto exit;
    }

    if((Ret = ThreadDb.Finalize(&ThreadDb)) == 0) {
        SdbLogInfo("Thread %ld: Database successfully finalized", DbmCtx->ThreadId);
    } else {
        SdbLogError("Thread %ld: Database finilization failed", DbmCtx->ThreadId);
    }

exit:
    DbmCtx->Errno = Ret;
    return NULL;
}
