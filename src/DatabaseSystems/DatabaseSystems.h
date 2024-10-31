#ifndef DATABASE_SYSTEMS_H
#define DATABASE_SYSTEMS_H

#include <src/Sdb.h>

SDB_BEGIN_EXTERN_C

#include <src/Common/SensorDataPipe.h>
#include <src/Libs/cJSON/cJSON.h>

typedef enum
{
    Dbs_Postgres = 1,

} Db_System_Type;

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
    u64                SensorCount;
    sensor_data_pipe **SdPipes;

    u64       ArgSize; // TODO(ingar): So the protocol can reclaim the memory used for OptArgs
    void     *OptArgs;
    void     *Ctx;
    sdb_arena Arena;
};

typedef sdb_errno (*dbs_init_api)(Db_System_Type DbsType, u64 SensorCount, sensor_data_pipe **Pipes,
                                  sdb_arena *Arena, u64 ArenaSize, i64 DbmTId, database_api *Dbs);

typedef struct
{
    sdb_barrier *ModulesBarrier;

    Db_System_Type DbsType;
    dbs_init_api   InitApi;

    sensor_data_pipe **SdPipes;
    u64                SensorCount; // NOTE(ingar): 1 pipe per sensor

    sdb_thread_control Control;

    u64       ArenaSize;
    u64       DbsArenaSize;
    sdb_arena Arena;

} db_module_ctx;


bool DbsDatabaseIsAvailable(Db_System_Type DbsId);

sdb_errno DbsInitApi(Db_System_Type DbsType, u64 SensorCount, sensor_data_pipe **Pipes,
                     sdb_arena *Arena, u64 ArenaSize, i64 DbmTId, database_api *Dbs);

db_module_ctx *DbModuleInit(sdb_barrier *ModulesBarrier, Db_System_Type Type, dbs_init_api ApiInit,
                            sensor_data_pipe **Pipes, u64 SensorCount, u64 ModuleArenaSize,
                            u64 DbsArenaSize, sdb_arena *Arena);

sdb_errno DbModuleRun(sdb_thread *DbmThread);

SDB_END_EXTERN_C

#endif
