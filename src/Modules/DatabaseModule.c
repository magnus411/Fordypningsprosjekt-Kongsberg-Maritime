#include <arpa/inet.h>
#include <iconv.h>
#include <libpq-fe.h>
#include <stdio.h>

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
    db_module_ctx *DbmCtx = DbmCtx_;
    DbmCtx->Errno         = 0;

    database_api ThreadDb;
    if(DbsInitApi(DbmCtx->DbsToRun, &DbmCtx->SdPipe, &DbmCtx->Arena, SdbMebiByte(8), &ThreadDb)
       == -SDBE_DBS_UNAVAIL) {
        SdbLogError("Thread %ld: Attempting to run %s, but its API is unavailable",
                    DbmCtx->ThreadId, DbsIdToName(DbmCtx->DbsToRun));
        DbmCtx->Errno = -SDBE_DBS_UNAVAIL;
        return NULL;
    }

    i64       Attempts = 0;
    sdb_errno Ret      = 0;
    while(((Ret = ThreadDb.Init(&ThreadDb)) != 0) && (Attempts++ < DB_INIT_ATTEMPT_THRESHOLD)) {
        SdbLogError("Thread %ld: Error on database init attempt %ld, ret: %d", DbmCtx->ThreadId,
                    Attempts, Ret);
    }

    if(Attempts >= DB_INIT_ATTEMPT_THRESHOLD) {
        SdbLogError("Thread %ld: Database init attempt threshold exceeded", DbmCtx->ThreadId);
        DbmCtx->Errno = Ret;
        return NULL;
    } else {
        SdbLogInfo("Thread %ld: Database successfully initialized", DbmCtx->ThreadId);
    }

    while((Ret = ThreadDb.Run(&ThreadDb)) != 0) {
        // TODO(ingar): Error/warning/whatever handling
    }

    if(Ret == 0) {
        SdbLogInfo("Thread %ld: Database has stopped and returned success", DbmCtx->ThreadId);
    } else {
        SdbLogError("Thread %ld: Database has stopped and returned an error: %d", DbmCtx->ThreadId,
                    Ret);
        DbmCtx->Errno = Ret;
    }

    Ret = ThreadDb.Finalize(&ThreadDb);
    if(Ret == 0) {
        SdbLogInfo("Thread %ld: Database successfully finalized", DbmCtx->ThreadId);
        DbmCtx->Errno = 0;
    } else {
        SdbLogError("Thread %ld: Database finilization failed", DbmCtx->ThreadId);
        DbmCtx->Errno = Ret;
    }

    return NULL;
}
