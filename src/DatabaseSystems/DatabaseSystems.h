#ifndef DATABASE_SYSTEMS_H
#define DATABASE_SYSTEMS_H

#include <src/Sdb.h>

SDB_BEGIN_EXTERN_C

#include <src/Common/Errno.h>
#include <src/Common/SensorDataPipe.h>
#include <src/Libs/cJSON/cJSON.h>

typedef enum
{
    Dbs_Postgres = 1,

} Db_System_Type;

// NOTE(ingar): The API design is inspired by
// https://docs.zephyrproject.org/apidoc/latest/log__backend_8h_source.html

typedef struct database_api database_api;
struct database_api
{
    sdb_errno (*Init)(database_api *Db);
    sdb_errno (*Run)(database_api *Db);
    sdb_errno (*Finalize)(database_api *Db);

    // NOTE(ingar): For now, the db system can define a macro that casts the context to their own
    // context type. I don't know of a better solution atm
    sensor_data_pipe SdPipe;
    sdb_arena        Arena;
    void            *Ctx;
    void            *OptArgs;
};

typedef sdb_errno (*dbs_init_api)(Db_System_Type DbsType, sensor_data_pipe *SdPipe,
                                  sdb_arena *Arena, u64 ArenaSize, i64 DbmTId, database_api *Dbs);

bool DbsDatabaseIsAvailable(Db_System_Type DbsId);

sdb_errno DbsInitApi(Db_System_Type DbsType, sensor_data_pipe *SdPipe, sdb_arena *Arena,
                     u64 ArenaSize, i64 DbmTId, database_api *Dbs);

static inline const char *
DbsTypeToName(Db_System_Type Type)
{
    switch(Type) {
        case Dbs_Postgres:
            return "Postgres";
        default:
            return "Dbs does not exist";
    }
}
SDB_END_EXTERN_C

#endif
