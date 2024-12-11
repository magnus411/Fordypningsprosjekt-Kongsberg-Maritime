#ifndef THREAD_GROUP_H
#define THREAD_GROUP_H
#include <pthread.h>
#include <stdbool.h>

#include <src/Sdb.h>

#include <src/Common/Thread.h>

typedef void *(*tg_task)(void *);
typedef void *(*tg_init)(void);
typedef sdb_errno (*tg_cleanup)(void *);

typedef struct tg_manager tg_manager;
typedef struct
{
    pthread_t *Threads;
    void      *SharedData;

    u64  GroupId;
    u64  ThreadCount;
    bool Completed;

    tg_manager *Manager;
    tg_cleanup  Cleanup;
    tg_task    *Tasks;

} tg_group;

struct tg_manager
{
    tg_group **Groups;

    u64 GroupCount;
    u64 CompletedCount;

    sdb_mutex Mutex;
    sdb_cond  Cond;
};

tg_manager *TgCreateManager(tg_group **Groups, u64 GroupCount, sdb_arena *A);
void        TgDestroyManager(tg_manager *M);
tg_group   *TgCreateGroup(u64 GroupId, u64 ThreadCount, void *SharedData, tg_init Init,
                          tg_task *Tasks, tg_cleanup Cleanup, sdb_arena *A);
sdb_errno   TgStartGroup(tg_group *Group);
sdb_errno   TgManagerStartAll(tg_manager *Manager);
void        TgManagerWaitForAll(tg_manager *Manager);

#endif
