#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>

#include <src/Sdb.h>
SDB_LOG_REGISTER(ThreadGroup);

#include <src/Common/Thread.h>
#include <src/Common/ThreadGroup.h>
#include <src/Common/Time.h>

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

    if(Group->Cleanup) {
        Group->Cleanup(Group->SharedData);
    }

    SdbMutexLock(&Group->Manager->Mutex, SDB_TIMEOUT_MAX);
    Group->Completed = true;
    Group->Manager->CompletedCount++;
    SdbCondSignal(&Group->Manager->Cond);
    SdbMutexUnlock(&Group->Manager->Mutex);

    return NULL;
}

tg_group *
TgCreateGroup(tg_manager *Manager, i32 GroupId, i32 ThreadCount, void *SharedData, tg_init Init,
              tg_task *Tasks, tg_cleanup Cleanup, sdb_arena *A)
{
    tg_group *Group = SdbPushStruct(A, tg_group);
    if(!Group) {
        return NULL;
    }

    Group->Threads = SdbPushArray(A, pthread_t, ThreadCount);
    if(!Group->Threads) {
        return NULL;
    }

    Group->GroupId     = GroupId;
    Group->ThreadCount = ThreadCount;
    Group->Completed   = false;
    Group->Manager     = Manager;
    Group->Cleanup     = Cleanup;
    Group->SharedData  = (!SharedData && Init) ? Init() : SharedData;
    Group->Tasks       = SdbPushArray(A, tg_task, ThreadCount); // To improve data locality
    if(Group->Tasks == NULL) {
        return NULL;
    } else {
        SdbMemcpy(Group->Tasks, Tasks, ThreadCount * sizeof(*Tasks));
    }

    return Group;
}

sdb_errno
TgStartGroup(tg_group *Group)
{
    sdb_errno Ret = 0;

    for(i32 i = 0; i < Group->ThreadCount; ++i) {
        Ret = pthread_create(&Group->Threads[i], NULL, Group->Tasks[i], Group->SharedData);
        if(Ret != 0) {
            for(i32 j = 0; j < i; ++j) {
                pthread_join(Group->Threads[j], NULL);
            }
            if(Group->Cleanup) {
                Group->Cleanup(Group->SharedData);
            }
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

sdb_errno
TgManagerStartAll(tg_manager *Manager)
{
    sdb_errno Ret = 0;
    SdbMutexLock(&Manager->Mutex, SDB_TIMEOUT_MAX);

    for(u64 g = 0; g < Manager->GroupCount; ++g) {
        Ret = TgStartGroup(Manager->Groups[g]);
        if(Ret != 0) {
            SdbLogError("Manager failed to start group %d", Manager->Groups[g]->GroupId);
            break;
        }
    }

    SdbMutexUnlock(&Manager->Mutex);
    return Ret;
}

void
TgManagerWaitForAll(tg_manager *Manager)
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
