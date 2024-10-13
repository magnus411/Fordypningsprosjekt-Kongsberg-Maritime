#include <DatabaseSystems/DatabaseSystems.h>
#include <Modules/DatabaseModule.h>

// This is used in DatabaseModule.c by referencing the database systems using extern

#ifdef DATABASE_SYSTEM_POSTGRES
static database_api PgApi__
    = { .Init = PgInit, .Run = PgRun, .Context = NULL, .IsInitialized = false };
database_api *DbSystemPostgres__ = &PgApi__;
#else
database_api *DbPostgres__ = NULL;
#endif
