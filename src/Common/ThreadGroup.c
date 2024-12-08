#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

#include <src/Sdb.h>
SDB_LOG_REGISTER(ThreadGroup);

#include <src/Common/Thread.h>
#include <src/Common/ThreadGroup.h>
#include <src/Common/Time.h>

struct tg_manager;

typedef void *(*tg_fn)(void *);

typedef struct
{
    pthread_t *Threads;
    void      *SharedData;

    i32  GroupId;
    i32  ThreadCount;
    bool Completed;

    struct tg_manager *Manager;

    void (*CleanupFn)(void *);

} tg_group;


typedef struct tg_manager
{
    tg_group **Groups;

    i32 GroupCount;
    i32 CompletedCount;

    sdb_mutex Mutex;
    sdb_cond  Cond;

} tg_manager;

sdb_errno
TgInitManager(tg_manager *Manager, i32 MaxGroups, sdb_arena *A)
{
    Manager->Groups = SdbPushArray(A, tg_group *, MaxGroups);
    if(!Manager->Groups) {
        return -ENOMEM;
    }

    Manager->GroupCount     = MaxGroups;
    Manager->CompletedCount = 0;
    SdbMutexInit(&Manager->Mutex);
    SdbCondInit(&Manager->Cond);

    return 0;
}

void *
TgMonitor(void *Arg)
{
    tg_group *Group = (tg_group *)Arg;

    for(i32 i = 0; i < Group->ThreadCount; ++i) {
        pthread_join(Group->Threads[i], NULL);
    }

    if(Group->CleanupFn) {
        Group->CleanupFn(Group->SharedData);
    }

    SdbMutexLock(&Group->Manager->Mutex, SDB_TIMEOUT_MAX);
    Group->Completed = true;
    Group->Manager->CompletedCount++;
    SdbCondSignal(&Group->Manager->Cond);
    SdbMutexUnlock(&Group->Manager->Mutex);

    return NULL;
}

sdb_errno
TgInitGroup(tg_group *Group, tg_manager *Manager, i32 GroupId, i32 ThreadCount,
            void *(*InitFn)(void), tg_fn *Tasks, void (*CleanupFn)(void *), sdb_arena *A)
{
    sdb_errno Ret = 0;

    Group->Threads     = SdbPushArray(A, pthread_t, ThreadCount);
    Group->GroupId     = GroupId;
    Group->ThreadCount = ThreadCount;
    Group->Completed   = false;
    Group->Manager     = Manager;
    Group->CleanupFn   = CleanupFn;

    if(!Group->Threads) {
        return -ENOMEM;
    }

    Group->SharedData = InitFn ? InitFn() : NULL;

    for(i32 i = 0; i < ThreadCount; ++i) {
        Ret = pthread_create(&Group->Threads[i], NULL, Tasks[i], Group->SharedData);
        if(Ret != 0) {
            for(i32 j = 0; j < i; ++j) {
                pthread_join(Group->Threads[j], NULL);
            }
            if(CleanupFn) {
                CleanupFn(Group->SharedData);
            }
            free(Group->Threads);
            return -Ret;
        }
    }

    pthread_t MonitorThread;
    Ret = pthread_create(&MonitorThread, NULL, TgMonitor, Group);
    if(Ret != 0) {
        return -Ret;
    }
    pthread_detach(MonitorThread);

    return Ret;
}

void
TgWaitForAll(tg_manager *Manager)
{
    SdbMutexLock(&Manager->Mutex, SDB_TIMEOUT_MAX);

    while(Manager->CompletedCount < Manager->GroupCount) {
        SdbCondWait(&Manager->Cond, &Manager->Mutex, SDB_TIMEOUT_MAX);

        for(i32 i = 0; i < Manager->GroupCount; ++i) {
            tg_group *Group = Manager->Groups[i];
            if(Group && Group->Completed) {
                SdbLogInfo("Group %d completed", Group->GroupId);
                Manager->Groups[i] = NULL;
            }
        }
    }

    SdbMutexUnlock(&Manager->Mutex);
}
