#ifndef DATABASE_SYSTEMS_H
#define DATABASE_SYSTEMS_H

#define DATABASE_SYSTEM_POSTGRES 1

#define USED_DATABASE_SYSTEM 1

#if USED_DATABASE_SYSTEM == DATABASE_SYSTEM_POSTGRES
#include <database_systems/Postgres.h>
#else
#error "Database system not supported"
#endif
#endif
