#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <time.h>
/**
 * @file Thread.c
 * @brief Implementation of POSIX Thread Synchronization Primitives
 *
 * Provides a unified, error-handled interface for POSIX
 * threading synchronization mechanisms.
 *
 * Key Features:
 * - Semaphore management
 * - Mutex operations
 * - Condition variable handling
 * - Thread control utilities
 *
 * Design Principles:
 * - Consistent error handling
 * - Timeout-based blocking primitives
 * - Minimal abstraction overhead
 *
 */

#include <src/Sdb.h>
SDB_LOG_REGISTER(Thread);

#include <src/Common/Thread.h>
#include <src/Common/Time.h>

// NOTE(ingar): This is totally not just directly copied from oec. Not at all

static sdb_errno
CreateTimeout(struct timespec *Timespec, sdb_timediff Timeout)
{
    sdb_errno Ret = SdbTimespecAbsolute(Timespec, Timeout);
    return Ret;
}

/* Semaphore */
sdb_errno
SdbSemInit(sdb_sem *Sem, u32 InitialValue)
{
    if(sem_init(Sem, 0, InitialValue) != 0) {
        return -errno;
    }

    return 0;
}

sdb_errno
SdbSemDeinit(sdb_sem *Sem)
{
    (void)Sem;
    return 0;
}

sdb_errno
SdbSemWait(sdb_sem *Sem, sdb_timediff Timeout)
{
    sdb_errno Ret = 0;
    if(SDB_TIME_EQ(Timeout, SDB_TIMEOUT_NONE)) {
        Ret = sem_trywait(Sem);
    } else if(SDB_TIME_EQ(Timeout, SDB_TIMEOUT_MAX)) {
        Ret = sem_wait(Sem);
    } else {
        struct timespec Timespec;
        Ret = CreateTimeout(&Timespec, Timeout);
        if(Ret != 0) {
            return Ret;
        }

        Ret = sem_timedwait(Sem, &Timespec);
    }

    if(Ret != 0) {
        return -errno;
    }

    return 0;
}

sdb_errno
SdbSemPost(sdb_sem *Sem)
{
    if(sem_post(Sem) != 0) {
        return -errno;
    }

    return 0;
}

/*Mutex */
sdb_errno
SdbMutexInit(sdb_mutex *Mutex)
{
    sdb_errno Ret = -pthread_mutex_init(Mutex, NULL);
    return Ret;
}

sdb_errno
SdbMutexDeinit(sdb_mutex *Mutex)
{
    sdb_errno Ret = -pthread_mutex_destroy(Mutex);
    return Ret;
}

sdb_errno
SdbMutexLock(sdb_mutex *Mutex, sdb_timediff Timeout)
{
    sdb_errno Ret = 0;
    if(SDB_TIME_EQ(Timeout, SDB_TIMEOUT_NONE)) {
        Ret = pthread_mutex_trylock(Mutex);
    } else if(SDB_TIME_EQ(Timeout, SDB_TIMEOUT_MAX)) {
        Ret = pthread_mutex_lock(Mutex);
    } else {
        struct timespec Timespec;
        Ret = CreateTimeout(&Timespec, Timeout);
        if(Ret != 0) {
            return Ret;
        }

        Ret = pthread_mutex_timedlock(Mutex, &Timespec);
    }

    return -Ret;
}

sdb_errno
SdbMutexUnlock(sdb_mutex *Mutex)
{
    sdb_errno Ret = -pthread_mutex_unlock(Mutex);
    return Ret;
}

/*Condition variable */
sdb_errno
SdbCondInit(sdb_cond *Cond)
{
    sdb_errno Ret = -pthread_cond_init(Cond, NULL);
    return Ret;
}

sdb_errno
SdbCondDeinit(sdb_cond *Cond)
{
    sdb_errno Ret = -pthread_cond_destroy(Cond);
    return Ret;
}

sdb_errno
SdbCondWait(sdb_cond *Cond, sdb_mutex *Mutex, sdb_timediff Timeout)
{
    sdb_errno Ret = 0;
    if(SDB_TIME_EQ(Timeout, SDB_TIMEOUT_NONE)) {
        return -EINVAL;
    } else if(SDB_TIME_EQ(Timeout, SDB_TIMEOUT_MAX)) {
        Ret = pthread_cond_wait(Cond, Mutex);
    } else {
        struct timespec Timespec;
        Ret = CreateTimeout(&Timespec, Timeout);
        if(Ret != 0) {
            return Ret;
        }

        Ret = pthread_cond_timedwait(Cond, Mutex, &Timespec);
    }

    return -Ret;
}

sdb_errno
SdbCondSignal(sdb_cond *Cond)
{
    sdb_errno Ret = -pthread_cond_signal(Cond);
    return Ret;
}

sdb_errno
SdbCondBroadcast(sdb_cond *Cond)
{
    SdbLogDebug("Broadcasting on condition variable %p", (void *)Cond);
    sdb_errno Ret = -pthread_cond_broadcast(Cond);
    if(Ret != 0) {
        SdbLogError("Broadcast failed with error: %s", strerror(-Ret));
    }
    return Ret;
}

/* Barrier*/
sdb_errno
SdbBarrierInit(sdb_barrier *Barrier, u32 ThreadCount)
{
    if(ThreadCount == 0) {
        return -EINVAL;
    }

    sdb_errno Ret = -pthread_barrier_init(Barrier, NULL, ThreadCount);
    return Ret;
}

sdb_errno
SdbBarrierDeinit(sdb_barrier *Barrier)
{
    sdb_errno Ret = -pthread_barrier_destroy(Barrier);
    return Ret;
}

sdb_errno
SdbBarrierWait(sdb_barrier *Barrier /*timeout*/)
{
    sdb_errno Ret = -pthread_barrier_wait(Barrier);
    return Ret;
}


sdb_errno
SdbTCtlInit(sdb_thread_control *Control)
{
    Control->HasStopped           = false;
    Control->ShouldStop           = false;
    Control->WaitingForSignalStop = false;

    // Initialize in order, checking each step
    sdb_errno MutRet = SdbMutexInit(&Control->Mutex);
    if(MutRet != 0) {
        return MutRet;
    }

    sdb_errno CondRet = SdbCondInit(&Control->Cond);
    if(CondRet != 0) {
        SdbMutexDeinit(&Control->Mutex); // Clean up mutex if cond init fails
        return CondRet;
    }

    return 0;
}

sdb_errno
SdbTCtlDeinit(sdb_thread_control *Control)
{
    sdb_errno CondRet = SdbCondDeinit(&Control->Cond);
    sdb_errno MutRet  = SdbMutexDeinit(&Control->Mutex);
    return (CondRet != 0) ? CondRet : MutRet;
}

bool
SdbTCtlShouldStop(sdb_thread_control *Control)
{
    bool ShouldStop;
    SdbMutexLock(&Control->Mutex, SDB_TIMEOUT_MAX);
    ShouldStop = Control->ShouldStop;
    SdbMutexUnlock(&Control->Mutex);
    return ShouldStop;
}

bool
SdbTCtlShouldStopLocked(sdb_thread_control *Control)
{
    return Control->ShouldStop;
}

bool
SdbTCtlHasStoppedLocked(sdb_thread_control *Control)
{
    return Control->HasStopped;
}

sdb_errno
SdbTCtlSignalStop(sdb_thread_control *Control)
{
    SdbMutexLock(&Control->Mutex, SDB_TIMEOUT_MAX);
    Control->ShouldStop = true;
    if(Control->WaitingForSignalStop) {
        SdbCondBroadcast(&Control->Cond);
    }
    SdbMutexUnlock(&Control->Mutex);
    return 0;
}

sdb_errno
SdbTCtlWaitForSignal(sdb_thread_control *Control)
{
    SdbMutexLock(&Control->Mutex, SDB_TIMEOUT_MAX);
    while(!SdbTCtlShouldStopLocked(Control)) {
        Control->WaitingForSignalStop = true;
        SdbCondWait(&Control->Cond, &Control->Mutex, SDB_TIMEOUT_MAX);
    }
    SdbMutexUnlock(&Control->Mutex);

    return 0;
}

sdb_errno
SdbTCtlMarkStopped(sdb_thread_control *Control)
{
    sdb_errno Ret = SdbMutexLock(&Control->Mutex, SDB_TIMEOUT_MAX);
    if(Ret != 0) {
        SdbLogError("Failed to lock mutex in MarkStopped: %d", Ret);
    }
    Control->HasStopped = true;
    if(Control->WaitingForMarkStopped) {
        SdbCondBroadcast(&Control->Cond);
    }
    SdbMutexUnlock(&Control->Mutex);

    return 0;
}

sdb_errno
SdbTCtlWaitForStop(sdb_thread_control *Control)
{
    SdbMutexLock(&Control->Mutex, SDB_TIMEOUT_MAX);
    while(!SdbTCtlHasStoppedLocked(Control)) {
        Control->WaitingForMarkStopped = true;
        SdbCondWait(&Control->Cond, &Control->Mutex, SDB_TIMEOUT_MAX);
    }
    SdbMutexUnlock(&Control->Mutex);

    return 0;
}
