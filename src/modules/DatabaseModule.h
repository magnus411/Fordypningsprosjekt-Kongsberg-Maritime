#ifndef DATABASE_MODULE_H_
#define DATABASE_MODULE_H_

#include <libpq-fe.h>
#include <stdatomic.h>

#include <SdbExtern.h>

// NOTE(ingar): The API design is inspired by
// https://docs.zephyrproject.org/apidoc/latest/log__backend_8h_source.html

// NOTE(ingar): We're currently working with the assumption that there will only be one database
// used at a time, so the API can be registered at compile time without conflict

typedef struct database_api database_api;
struct database_api
{
    // sdb_errno (*Insert)(database_api *Db);
    sdb_errno (*Init)(database_api *Db);
    sdb_errno (*Run)(database_api *Db);

    atomic_bool IsInitialized;
    void       *Context;
};

/**
 * @brief Context for a thread running DbModuleRun
 * @param Errno Errno value set by DbModuleRun to indicate error/success
 * @param ThreadId
 */
typedef struct
{
    i64 ThreadId;

    sdb_errno Errno;

} db_module_ctx;

/**
 * @brief Database module's main function, which should be spawned in a thread.
 *
 * @param DbModulectx Pointer to a db_module_ctx
 * @retval Always NULL, check @p DbModuleCtx for errno
 */
void *DbModuleRun(void *DbModuleCtx);

#endif
