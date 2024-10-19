#ifndef DATABASE_MODULE_H_
#define DATABASE_MODULE_H_

#include <libpq-fe.h>

#include <src/Sdb.h>

#include <src/DatabaseSystems/DatabaseSystems.h>
#include <src/Libs/cJSON/cJSON.h>

typedef struct
{
    i64            ThreadId;
    sdb_errno      Errno;
    Db_System_Type DbsType;

    sensor_data_pipe SdPipe;

    sdb_arena Arena;
    u64       ArenaSize;
    u64       DbsArenaSize;

} db_module_ctx;

/**
 * @brief Database module's main function, which should be spawned in a thread.
 *
 * @param DbModulectx Pointer to a db_module_ctx
 * @retval Always NULL, check @p DbModuleCtx for errno
 */
void *DbModuleRun(void *DbModuleCtx);

#endif
