#ifndef DATABASE_SYSTEMS_H
#define DATABASE_SYSTEMS_H

#include <src/Sdb.h>

#include <src/Common/SdbErrno.h>
#include <src/Common/SensorDataPipe.h>

typedef enum
{
    Dbs_Postgres = 1,

} Db_System_Id;

static inline const char *
DbsIdToName(Db_System_Id Id)
{
    switch(Id) {
        case Dbs_Postgres:
            return "Postgres";
        default:
            return "Dbs does not exist";
    }
}

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
    void            *Ctx;
    sensor_data_pipe SdPipe;
    sdb_arena        Arena;
};

sdb_errno DbsInitApi(Db_System_Id DbsId, sensor_data_pipe *SdPipe, sdb_arena *Arena, u64 ArenaSize,
                     database_api *Dbs);

#endif
