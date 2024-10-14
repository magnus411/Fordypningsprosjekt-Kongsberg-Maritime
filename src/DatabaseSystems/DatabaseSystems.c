#include <src/DatabaseSystems/DatabaseSystems.h>
#include <src/DatabaseSystems/Postgres.h>
#include <src/Modules/DatabaseModule.h>

// This is used in DatabaseModule.c by referencing the database systems using extern

#if DATABASE_SYSTEM_POSTGRES == 1
static database_api PgApi__
    = { .Init = PgInit, .Run = PgRun, .Finalize = PgFinalize, .Ctx = NULL, .IsInitialized = false };
database_api *DbSystemPostgres__ = &PgApi__;
#else
database_api *DbPostgres__ = NULL;
#endif // DATABASE_SYSTEM_POSTGRES
