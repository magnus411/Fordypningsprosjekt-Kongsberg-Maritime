#include "src/Common/Thread.h"
#include <libpq-fe.h>

#include <src/Sdb.h>

SDB_LOG_REGISTER(PostgresTest);
SDB_THREAD_ARENAS_EXTERN(Postgres);

#include <src/DatabaseSystems/DatabaseInitializer.h>
#include <src/DatabaseSystems/Postgres.h>
#include <src/Libs/cJSON/cJSON.h>

#include <tests/DatabaseSystems/PostgresTest.h>
#include <tests/TestConstants.h>

sdb_errno
PgInitTest(database_api *Pg)
{
    PgInitThreadArenas();
    SdbThreadArenasInitExtern(Postgres);
    
    // Initialize main arena and scratch arenas
    sdb_arena *Scratch1 = SdbArenaBootstrap(&Pg->Arena, NULL, SdbKibiByte(512));
    sdb_arena *Scratch2 = SdbArenaBootstrap(&Pg->Arena, NULL, SdbKibiByte(512));
    SdbThreadArenasAdd(Scratch1);
    SdbThreadArenasAdd(Scratch2);
    
    // Initialize postgres context
    postgres_ctx *PgCtx = SdbPushStruct(&Pg->Arena, postgres_ctx);
    PgCtx->Threads = SdbPushArray(&Pg->Arena, sdb_thread, Pg->SensorCount);
    PgCtx->ThreadControls = SdbPushArray(&Pg->Arena, sdb_thread_control, Pg->SensorCount);
    PgCtx->ThreadContexts = SdbPushArray(&Pg->Arena, pg_thread_ctx *, Pg->SensorCount);
    
    // Initialize all thread controls before any other usage
    for (u64 t = 0; t < Pg->SensorCount; ++t) {
        SdbTCtlInit(&PgCtx->ThreadControls[t]);
    }
    
    Pg->Ctx = PgCtx;
    
    if (PgPrepareThreads(Pg) != 0) {
        return -1;
    }
    
    // Create threads only after all initialization is complete
    for (u64 t = 0; t < Pg->SensorCount; ++t) {
        sdb_thread *Thread = &PgCtx->Threads[t];
        pg_thread_ctx *ThreadCtx = PgCtx->ThreadContexts[t];
        
        // Verify thread context setup
        if (!ThreadCtx || !ThreadCtx->Control) {
            SdbLogError("Invalid thread context setup for sensor idx %lu", t);
            return -1;
        }
        
        sdb_errno TRet = SdbThreadCreate(Thread, PgSensorThread, ThreadCtx);
        if (TRet != 0) {
            SdbLogError("Failed to create Postgres sensor thread for sensor idx %lu", t);
            return TRet;
        }
    }
    
    return 0;
}

sdb_errno
PgRunTest(database_api *Pg)
{

    return 0;
}

sdb_errno
PgFinalizeTest(database_api *Pg)
{
    postgres_ctx *PgCtx = PG_CTX(Pg);

    for(u64 t = 0; t < Pg->SensorCount; ++t) {
        SdbTCtlSignalStop(&PgCtx->ThreadControls[t]);
        SdbLogDebug("Signaling stop for thread %lu", t);
    }

    for(u64 t = 0; t < Pg->SensorCount; ++t) {
        sdb_errno CtlRet = SdbTCtlWaitForStop(&PgCtx->ThreadControls[t]);
        if(CtlRet != 0) {
            SdbLogError("Threads failed to stop");
            return CtlRet;
        }

        sdb_errno TRet = SdbThreadJoin(&PG_CTX(Pg)->Threads[t]);
        if(TRet != 0) {
            SdbLogError("Threads failed to join");
            return TRet;
        }

        pg_thread_ctx *PtCtx = PgCtx->ThreadContexts[t];
        PQfinish(PtCtx->DbConn);
    }

    // SdbArenaClear(&Pg->Arena);
    return 0;
}
