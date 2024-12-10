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

    i32  GroupId;
    i32  ThreadCount;
    bool Completed;

    tg_manager *Manager;
    tg_cleanup  Cleanup;
    tg_task    *Tasks;

} tg_group;

struct tg_manager
{
    tg_group **Groups;

    i32 GroupCount;
    i32 CompletedCount;

    sdb_mutex Mutex;
    sdb_cond  Cond;
};

sdb_errno TgInitManager(tg_manager *Manager, i32 MaxGroups, sdb_arena *A);
tg_group *TgCreateGroup(tg_manager *Manager, i32 GroupId, i32 ThreadCount, void *SharedData,
                        tg_init Init, tg_task *Tasks, tg_cleanup Cleanup, sdb_arena *A);
sdb_errno TgStartGroup(tg_group *Group);
sdb_errno TgManagerStartAll(tg_manager *Manager);
void      TgManagerWaitForAll(tg_manager *Manager);

#endif
