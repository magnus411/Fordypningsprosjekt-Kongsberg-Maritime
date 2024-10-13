/*
 * Save to database
 * Read from buffers
 * Monitor database stats
 *   - Ability to prioritize data based on it
 *
 * ? Send messages to comm module ?
 * ? Metaprogramming tool to create C structs from sensor schemas ?
 */

#include <arpa/inet.h>
#include <iconv.h>
#include <libpq-fe.h>
#include <stdio.h>

#include <src/Sdb.h>

#include <src/Common/CircularBuffer.h>
#include <src/Common/SdbErrno.h>
#include <src/DatabaseSystems/DatabaseSystems.h>
#include <src/Modules/DatabaseModule.h>

SDB_LOG_REGISTER(DbModule);

#define DB_INIT_ATTEMPT_THRESHOLD (5)

// TODO(ingar): Figure out if there is some equivalent to zephyr sections if we
// want to support multiple database apis
extern database_api *DbSystemPostgres__;
// Add more systems as we use them

static inline bool
DbIsInitialized(database_api *Db)
{
    bool Expected = true;
    if(atomic_compare_exchange_strong(&Db->IsInitialized, &Expected, true)) {
        return true;
    } else {
        return false;
    }
}

void *
DbModuleRun(void *DbmCtx_)
{
    db_module_ctx *DbmCtx = DbmCtx_;
    DbmCtx->Errno         = 0;

    database_api ThreadDb;
    switch(DbmCtx->DbsToRun) {
        case Dbs_Postgres:
            {
                if(DbSystemPostgres__ == NULL) {
                    SdbLogError("Attempting to run PostgreSQL, but its API is unavailable");
                    DbmCtx->Errno = -SDBE_DBS_UNAVAIL;
                    return NULL;
                }
                memcpy(&ThreadDb, DbSystemPostgres__, sizeof(database_api));
            }
            break;
        default:
            {
                SdbLogError("Database id doesn't exist");
                DbmCtx->Errno = -EINVAL;
                return NULL;
            }
            break;
    }

    SdbArenaBootstrap(&DbmCtx->Arena, &ThreadDb.Arena, SdbMebiByte(8));
    SdbMemcpy(&ThreadDb.SdPipe, &DbmCtx->SdPipe, sizeof(sensor_data_pipe));

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
