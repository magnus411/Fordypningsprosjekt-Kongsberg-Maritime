/*
 * Save to database
 * Read from buffers
 * Monitor database stats
 *   - Ability to prioritize data based on it
 *
 * ? Send messages to comm module ?
 * ? Metaprogramming tool to create C structs from sensor schemas ?
 */

#include <libpq-fe.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <iconv.h>

#define SDB_LOG_LEVEL 4
#include <SdbExtern.h>

#include <database_systems/Postgres.h>
#include <modules/DatabaseModule.h>
#include <common/CircularBuffer.h>

SDB_LOG_REGISTER(DbModule);

#define DB_INIT_ATTEMPT_THRESHOLD (5)

// TODO(ingar): Figure out if there is some equivalent to zephyr sections if we want to support
// multiple database apis
extern database_api *DbDefault_;

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

static inline sdb_errno
DbInit(database_api *Db)
{
    sdb_errno Ret = Db->Init(Db);
    return Ret;
}

static inline sdb_errno
DbRun(database_api *Db)
{
    sdb_errno Ret = Db->Run(Db);
    return Ret;
}

void *
DbModuleRun(void *DbModuleCtx)
{
    db_module_ctx *DbmCtx = (db_module_ctx *)DbModuleCtx;
    DbmCtx->Errno         = 0;

    database_api ThreadDb;
    memcpy(&ThreadDb, DbDefault_, sizeof(database_api));

    i64       Attempts = 0;
    sdb_errno Ret      = 0;

    while(((Ret = DbInit(&ThreadDb)) != 0) && (Attempts++ < DB_INIT_ATTEMPT_THRESHOLD)) {
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

    while((Ret = DbRun(&ThreadDb)) != 0) {
        // TODO(ingar): Error/warning/whatever handling
    }

    if(Ret == 0) {
        SdbLogInfo("Thread %ld: Database has stopped and returned success", DbmCtx->ThreadId);
    } else {
        SdbLogError("Thread %ld: Database has stopped and returned an error: %d", DbmCtx->ThreadId,
                    Ret);
        DbmCtx->Errno = Ret;
    }

    return NULL;
}
