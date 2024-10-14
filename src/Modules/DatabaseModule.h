#ifndef DATABASE_MODULE_H_
#define DATABASE_MODULE_H_

#include <libpq-fe.h>

#include <src/Sdb.h>

#include <src/DatabaseSystems/DatabaseSystems.h>

typedef struct
{
    i64       ThreadId;
    sdb_errno Errno;

    Db_System_Id      DbsToRun;
    void             *DbsInitArgs;
    sensor_data_pipe *SdPipe;
    sdb_arena        *Arena;

} db_module_ctx;

typedef struct
{
    i64            DbCount;
    db_module_ctx *Databases;

} live_databases;

/**
 * @brief Database module's main function, which should be spawned in a thread.
 *
 * @param DbModulectx Pointer to a db_module_ctx
 * @retval Always NULL, check @p DbModuleCtx for errno
 */
void *DbModuleRun(void *DbModuleCtx);

#endif
