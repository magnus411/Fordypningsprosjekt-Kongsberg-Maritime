#ifndef DATABASE_SYSTEMS_H
#define DATABASE_SYSTEMS_H

typedef enum
{
    Dbs_Postgres = 1,

} Db_System_Id;

#if DATABASE_SYSTEM_POSTGRES == 1
#include <src/DatabaseSystems/Postgres.h>
#endif // DATABASE_SYSTEM_POSTGRES

#endif
