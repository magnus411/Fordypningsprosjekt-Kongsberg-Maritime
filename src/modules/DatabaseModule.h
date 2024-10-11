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
    sdb_errno (*Insert)(database_api *DbApi);
    sdb_errno (*Initialize)(database_api *DbApi);

    // TODO(ingar): Should the api have a lock?
    atomic_bool IsInitialized;
    void       *Context;
};

static inline bool
DbApiIsReady(database_api *DbApi)
{
    bool Expected = true;
    if(atomic_compare_exchange_strong(&DbApi->IsInitialized, &Expected, true)) {
        return true;
    } else {
        return false;
    }
}

static inline sdb_errno
DbInsert(database_api *DbApi)
{
    bool ApiIsReady = DbApiIsReady(DbApi);
    if(!ApiIsReady) {
        assert(ApiIsReady);
        return -1;
    }

    sdb_errno InsertRet = DbApi->Insert(DbApi);
    return InsertRet;
}

static inline sdb_errno
DbApiInit(database_api *DbApi)
{
    bool ApiIsReady = DbApiIsReady(DbApi);
    if(ApiIsReady) {
        return 0;
    }

    sdb_errno InitRet = DbApi->Initialize(DbApi);
    return InitRet;
}

#endif
