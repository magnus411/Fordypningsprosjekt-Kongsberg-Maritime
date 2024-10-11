#ifndef DATABASE_SYSTEMS_H
#define DATABASE_SYSTEMS_H

#define DATABASE_SYSTEM_POSTGRES 1

#ifndef DATABASE_SYSTEM
#define DATABASE_SYSTEM 1
#endif

#if DATABASE_SYSTEM == DATABASE_SYSTEM_POSTGRES
#include <database_systems/Postgres.h>
#else
#error "Database system not supported"
#endif
#endif
