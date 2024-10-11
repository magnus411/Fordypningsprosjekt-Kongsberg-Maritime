#include <database_systems/DatabaseSystems.h>

// This is used in DatabaseModule.c by referencing DbDefault_ as extern

#if DATABASE_SYSTEM == DATABASE_SYSTEM_POSTGRES
static database_api PgApi__
    = { .Init = PgInit, .Run = PgRun, .Context = NULL, .IsInitialized = false };
database_api *DbDefault_ = &PgApi__;
#else
#error "Database system not supported"
#endif
