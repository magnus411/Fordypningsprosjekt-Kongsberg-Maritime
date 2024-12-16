#ifndef SDB_THREAD_H
#define SDB_THREAD_H

/**
 * @file Thread.h
 * @brief Lightweight Synchronization Primitives Wrapper
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


#include <pthread.h>
#include <semaphore.h>

#include <src/Sdb.h>

SDB_BEGIN_EXTERN_C

#include <src/Common/Time.h>

typedef sem_t             sdb_sem;
typedef pthread_mutex_t   sdb_mutex;
typedef pthread_cond_t    sdb_cond;
typedef pthread_barrier_t sdb_barrier;


typedef struct
{
    sdb_mutex Mutex;
    sdb_cond  Cond;
    bool      ShouldStop;
    bool      HasStopped;
    bool      WaitingForSignalStop;
    bool      WaitingForMarkStopped;

} sdb_thread_control;


/**
 * @brief Initialize a semaphore
 * @param Sem Semaphore to initialize
 * @param InitialValue Initial semaphore value
 * @return sdb_errno Success or error code
 */
sdb_errno SdbSemInit(sdb_sem *Sem, u32 InitialValue);

/**
 * @brief Deinitialize a semaphore
 * @param Sem Semaphore to deinitialize
 * @return sdb_errno Success or error code
 */
sdb_errno SdbSemDeinit(sdb_sem *Sem);

/**
 * @brief Wait on a semaphore with timeout
 * @param Sem Semaphore to wait on
 * @param Timeout Maximum wait time
 * @return sdb_errno Success or error code
 */
sdb_errno SdbSemWait(sdb_sem *Sem, sdb_timediff Timeout);


/**
 * @brief Post (increment) a semaphore
 * @param Sem Semaphore to post
 * @return sdb_errno Success or error code
 */
sdb_errno SdbSemPost(sdb_sem *Sem);

/**
 * @brief Initialize a mutex
 * @param Mutex Mutex to initialize
 * @return sdb_errno Success or error code
 */
sdb_errno SdbMutexInit(sdb_mutex *Mutex);
/**
 * @brief Deinitialize a mutex
 * @param Mutex Mutex to deinitialize
 * @return sdb_errno Success or error code
 */
sdb_errno SdbMutexDeinit(sdb_mutex *Mutex);
/**
 * @brief Lock a mutex with timeout
 * @param Mutex Mutex to lock
 * @param Timeout Maximum wait time
 * @return sdb_errno Success or error code
 */
sdb_errno SdbMutexLock(sdb_mutex *Mutex, sdb_timediff Timeout);
/**
 * @brief Unlock a mutex
 * @param Mutex Mutex to unlock
 * @return sdb_errno Success or error code
 */
sdb_errno SdbMutexUnlock(sdb_mutex *Mutex);


/**
 * @brief Initialize a condition variable
 * @param Cond Condition variable to initialize
 * @return sdb_errno Success or error code
 */
sdb_errno SdbCondInit(sdb_cond *Cond);
/**
 * @brief Deinitialize a condition variable
 * @param Cond Condition variable to deinitialize
 * @return sdb_errno Success or error code
 */
sdb_errno SdbCondDeinit(sdb_cond *Cond);
/**
 * @brief Wait on a condition variable with timeout
 * @param Cond Condition variable to wait on
 * @param Mutex Associated mutex
 * @param Timeout Maximum wait time
 * @return sdb_errno Success or error code
 */
sdb_errno SdbCondWait(sdb_cond *Cond, sdb_mutex *Mutex, sdb_timediff Timeout);
/**
 * @brief Signal a condition variable
 * @param Cond Condition variable to signal
 * @return sdb_errno Success or error code
 */
sdb_errno SdbCondSignal(sdb_cond *Cond);
/**
 * @brief Broadcast on a condition variable
 * @param Cond Condition variable to broadcast
 * @return sdb_errno Success or error code
 */
sdb_errno SdbCondBroadcast(sdb_cond *Cond);


/**
 * @brief Initialize a thread barrier
 * @param Barrier Barrier to initialize
 * @param ThreadCount Number of threads to synchronize
 * @return sdb_errno Success or error code
 */
sdb_errno SdbBarrierInit(sdb_barrier *Barrier, u32 ThreadCount);
/**
 * @brief Deinitialize a thread barrier
 * @param Barrier Barrier to deinitialize
 * @return sdb_errno Success or error code
 */
sdb_errno SdbBarrierDeinit(sdb_barrier *Barrier);

/**
 * @brief Wait on a thread barrier
 * @param Barrier Barrier to wait on
 * @return sdb_errno Success or error code
 */
sdb_errno SdbBarrierWait(sdb_barrier *Barrier);

// TODO(ingar): Error handling
sdb_errno SdbTCtlInit(sdb_thread_control *Control);
sdb_errno SdbTCtlDeinit(sdb_thread_control *Control);
bool      SdbTCtlShouldStop(sdb_thread_control *Control);
bool      SdbTCtlShouldStopLocked(sdb_thread_control *Control);
bool      SdbTCtlHasStoppedLocked(sdb_thread_control *Control);
sdb_errno SdbTCtlSignalStop(sdb_thread_control *Control);
sdb_errno SdbTCtlWaitForSignal(sdb_thread_control *Control);
sdb_errno SdbTCtlMarkStopped(sdb_thread_control *Control);
sdb_errno SdbTCtlWaitForStop(sdb_thread_control *Control);

SDB_END_EXTERN_C

#endif
