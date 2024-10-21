#ifndef SDB_THREAD_H
#define SDB_THREAD_H

#include <pthread.h>
#include <semaphore.h>
#include <stdatomic.h>

#include <src/Sdb.h>

SDB_BEGIN_EXTERN_C

#include <src/Common/Errno.h>
#include <src/Common/Time.h>

typedef sem_t             sdb_sem;
typedef pthread_mutex_t   sdb_mutex;
typedef pthread_cond_t    sdb_cond;
typedef pthread_barrier_t sdb_barrier;

sdb_errno SdbSemInit(sdb_sem *Sem, u32 InitialValue);
sdb_errno SdbSemDeinit(sdb_sem *Sem);
sdb_errno SdbSemWait(sdb_sem *Sem, sdb_timediff Timeout);
sdb_errno SdbSemPost(sdb_sem *Sem);

sdb_errno SdbMutexInit(sdb_mutex *Mutex);
sdb_errno SdbMutexDeinit(sdb_mutex *Mutex);
sdb_errno SdbMutexLock(sdb_mutex *Mutex, sdb_timediff Timeout);
sdb_errno SdbMutexUnlock(sdb_mutex *Mutex);

sdb_errno SdbCondInit(sdb_cond *Cond);
sdb_errno SdbCondDeinit(sdb_cond *Cond);
sdb_errno SdbCondWait(sdb_cond *Cond, sdb_mutex *Mutex, sdb_timediff Timeout);
sdb_errno SdbCondSignal(sdb_cond *Cond);
sdb_errno SdbCondBroadcast(sdb_cond *Cond);

sdb_errno SdbBarrierInit(sdb_barrier *Barrier, u32 ThreadCount);
sdb_errno SdbBarrierDeinit(sdb_barrier *Barrier);
sdb_errno SdbBarrierWait(sdb_barrier *Barrier);

// TODO(ingar): rw_lock

struct sdb_thread;
typedef sdb_errno (*sdb_thread_task)(struct sdb_thread *Thread);
typedef struct sdb_thread
{
    pthread_t pid;

    bool Joinable;

    atomic_uint State;
    sdb_sem     NewSignal;
    sdb_sem     SignalProcessed;

    sdb_thread_task Task;
    sdb_errno       TaskResult;
    void           *Args;

} sdb_thread;

sdb_errno   SdbThreadCreate(sdb_thread *Thread, sdb_thread_task Task, void *Args);
sdb_errno   SdbThreadCheckSignal(sdb_thread *Thread, sdb_timediff MaxTimeout);
sdb_errno   SdbThreadJoin(sdb_thread *Thread);
sdb_errno   SdbThreadPause(sdb_thread *Thread, sdb_timediff MaxTimeout);
sdb_errno   SdbThreadContinue(sdb_thread *Thread, sdb_timediff MaxTimeout);
sdb_errno   SdbThreadKill(sdb_thread *Thread, sdb_timediff MaxTimeout);
sdb_thread *SdbThreadGetCurrent(void);

SDB_END_EXTERN_C

#endif
