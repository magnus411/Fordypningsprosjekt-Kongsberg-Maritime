#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <time.h>

#include <src/Sdb.h>
SDB_LOG_REGISTER(Thread);

#include <src/Common/SdbErrno.h>
#include <src/Common/SdbThread.h>
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
    sdb_errno Ret = -pthread_cond_broadcast(Cond);
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

/* Thread */

enum
{
    THREAD_CONTINUE_,
    THREAD_KILL_,
    THREAD_PAUSE_,
    THREAD_JOIN_,
};

static sdb_errno
ThreadSignal(sdb_thread *Thread, u32 Signal, sdb_timediff SignalProcessedTimeout)
{
    atomic_store(&Thread->State, Signal);

    sdb_errno Status = SdbSemPost(&Thread->NewSignal);
    if(Status != 0) {
        return Status;
    }

    Status = SdbSemWait(&Thread->SignalProcessed, SignalProcessedTimeout);
    return Status;
}

static sdb_errno
ThreadCheckSignal(sdb_thread *Thread, sdb_timediff NewSignalTimeout, sdb_timediff PauseTimeout)
{
    sdb_errno Status = SdbSemWait(&Thread->NewSignal, NewSignalTimeout);
    if(Status != 0) {
        if(Status == -EAGAIN) {
            Status = 0;
        }
        return Status;
    }

    do {
        u32 State = (u32)atomic_load(&Thread->State);
        Status    = SdbSemPost(&Thread->SignalProcessed);

        if(Status != 0) {
            break;
        }

        switch(State) {
            case THREAD_CONTINUE_:
                return 0;
            case THREAD_KILL_:
                return ECANCELED;
            case THREAD_JOIN_:
                Thread->Joinable = true;
                return 0;
            case THREAD_PAUSE_:
                break;
            default:
                SdbAssert(0, "Thread %lu was given unkown signal: %u", Thread->pid, Status);
        }

        // NOTE(ingar): A paused thread waits here
        Status = SdbSemWait(&Thread->NewSignal, PauseTimeout);
    } while(Status == 0);

    return Status;
}

static void *
TaskWrapper(void *Args)
{
    sdb_thread *Thread = Args;

    // Maybe add current thread like in oec

    Thread->TaskResult = Thread->Task(Thread);
    while(!Thread->Joinable) {
        ThreadCheckSignal(Thread, SDB_TIMEOUT_MAX, SDB_TIMEOUT_MAX);
    }

    return NULL;
}

sdb_errno
SdbThreadCreate(sdb_thread *Thread, sdb_thread_task Task, void *Args)
{
    SdbMemset(Thread, 0, sizeof(*Thread));
    Thread->Task = Task;
    Thread->Args = Args;

    sdb_errno Ret = SdbSemInit(&Thread->NewSignal, 0);
    if(Ret != 0) {
        return Ret;
    }

    Ret = SdbSemInit(&Thread->SignalProcessed, 0);
    if(Ret != 0) {
        return Ret;
    }

    Ret = -pthread_create(&Thread->pid, NULL, TaskWrapper, Thread->Args);
    return Ret;
}

sdb_errno
SdbThreadCheckSignal(sdb_thread *Thread, sdb_timediff MaxTimeout)
{
    sdb_errno Ret = ThreadCheckSignal(Thread, MaxTimeout, SDB_TIMEOUT_NONE);
    return Ret;
}

sdb_errno
SdbThreadJoin(sdb_thread *Thread)
{
    sdb_errno Ret = ThreadSignal(Thread, THREAD_JOIN_, SDB_TIMEOUT_MAX);
    if(Ret != 0) {
        return Ret;
    }

    Ret = pthread_join(Thread->pid, NULL);
    if(Ret != 0) {
        return Ret;
    }

    Ret = SdbSemDeinit(&Thread->SignalProcessed);
    if(Ret == 0) {
        Ret = SdbSemDeinit(&Thread->NewSignal);
    } else {
        (void)SdbSemDeinit(&Thread->NewSignal);
    }

    return (Ret == 0) ? Thread->TaskResult : Ret;
}

sdb_errno
SdbThreadPause(sdb_thread *Thread, sdb_timediff MaxTimeout)
{
    sdb_errno Ret = ThreadSignal(Thread, THREAD_PAUSE_, MaxTimeout);
    return Ret;
}

sdb_errno
SdbThreadContinue(sdb_thread *Thread, sdb_timediff MaxTimeout)
{
    sdb_errno Ret = ThreadSignal(Thread, THREAD_CONTINUE_, MaxTimeout);
    return Ret;
}

sdb_errno
SdbThreadKill(sdb_thread *Thread, sdb_timediff MaxTimeout)
{
    sdb_errno Ret = ThreadSignal(Thread, THREAD_KILL_, MaxTimeout);
    return Ret;
}
