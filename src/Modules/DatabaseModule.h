#ifndef DATABASE_MODULE_H_
#define DATABASE_MODULE_H_

#include <libpq-fe.h>
#include <stdatomic.h>

#include <src/Sdb.h>

#include <src/Common/SensorDataPipe.h>
#include <src/DatabaseSystems/DatabaseSystems.h>

// NOTE(ingar): The API design is inspired by
// https://docs.zephyrproject.org/apidoc/latest/log__backend_8h_source.html

typedef struct database_api database_api;
struct database_api
{
    sdb_errno (*Init)(database_api *Db);
    sdb_errno (*Run)(database_api *Db);
    sdb_errno (*Finalize)(database_api *Db);

    atomic_bool IsInitialized;
    void       *Ctx;
    // NOTE(ingar): For now, the db system can define a macro that casts the context to their own
    // context type. I don't know of a better solution atm

    sensor_data_pipe SdPipe;
    sdb_arena        Arena;
};

typedef struct
{
    i64           DbCount;
    database_api *Databases;

} live_databases;

typedef struct
{
    i64       ThreadId;
    sdb_errno Errno;

    Db_System_Id DbsToRun;
    void        *DbsInitArgs;

    sensor_data_pipe SdPipe;

    sdb_arena Arena;

} db_module_ctx;

/**
 * @brief Database module's main function, which should be spawned in a thread.
 *
 * @param DbModulectx Pointer to a db_module_ctx
 * @retval Always NULL, check @p DbModuleCtx for errno
 */
void *DbModuleRun(void *DbModuleCtx);

#endif
