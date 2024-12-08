#include <pthread.h>
#include <stdbool.h>

#include <src/Sdb.h>

#include <src/Common/Thread.h>

typedef void *(*tg_fn)(void *);

typedef struct tg_manager tg_manager;
typedef struct
{
    pthread_t *Threads;
    void      *SharedData;

    i32  GroupId;
    i32  ThreadCount;
    bool Completed;

    tg_manager *Manager;

    void (*CleanupFn)(void *);

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
sdb_errno TgInitGroup(tg_group *Group, tg_manager *Manager, i32 GroupId, i32 ThreadCount,
                      void *(*InitFn)(void), tg_fn *Tasks, void (*CleanupFn)(void *), sdb_arena *A);
void      TgWaitForAll(tg_manager *Manager);
