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
    PgCtx->TablesInfo   = SdbPushArray(&Pg->Arena, pg_table_info *, Pg->SensorCount);
    Pg->Ctx             = PgCtx;

    if(PgPrepare(Pg) != 0) {
        return -1;
    }


    return 0;
}

sdb_errno
PgRunTest(database_api *Pg)
{
    sdb_errno Ret = PgMainLoop(Pg);

    return Ret;
}

sdb_errno
PgFinalizeTest(database_api *Pg)
{
    postgres_ctx *PgCtx = PG_CTX(Pg);

    return 0;
}
