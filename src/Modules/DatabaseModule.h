#ifndef DATABASE_MODULE_H_
#define DATABASE_MODULE_H_

#include <libpq-fe.h>

#include <src/Sdb.h>

#include <src/Common/Thread.h>
#include <src/DatabaseSystems/DatabaseSystems.h>
#include <src/Libs/cJSON/cJSON.h>

typedef struct
{
    sdb_barrier *ModulesBarrier;

    Db_System_Type DbsType;
    dbs_init_api   InitApi;

    sensor_data_pipe SdPipe;

    sdb_arena Arena;
    u64       ArenaSize;
    u64       DbsArenaSize;

} db_module_ctx;

sdb_errno DbModuleRun(sdb_thread *DbmThread);

#endif
