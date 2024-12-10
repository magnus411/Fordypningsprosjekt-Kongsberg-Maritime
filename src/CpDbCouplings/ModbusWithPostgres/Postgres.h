#ifndef POSTGRES_API_H
#define POSTGRES_API_H

#include <src/Sdb.h>

#include <src/DatabaseSystems/DatabaseSystems.h>

sdb_errno PgInit(database_api *Pg);
sdb_errno PgRun(database_api *Pg);
sdb_errno PgFinalize(database_api *Pg);

#endif
