#ifndef THREAD_GROUP_H
#define THREAD_GROUP_H


/**
 * @file ThreadGroup.h
 * @brief Thread Group Management System
 *
 * Provides a high-level abstraction for managing groups of
 * concurrent threads with shared resources and synchronized lifecycle.
 *
 * Key Features:
 * - Dynamic thread group creation
 * - Shared data management
 * - Centralized thread lifecycle control
 * - Flexible initialization and cleanup mechanisms
 *
 * Design Principles:
 * - Minimal synchronization overhead
 * - Centralized thread group management
 * - Extensible thread creation patterns
 *
 */


#include <pthread.h>
#include <stdbool.h>

#include <src/Sdb.h>

#include <src/Common/Thread.h>


/**
 * @brief Thread task function signature
 *
 * Defines the function type for individual thread tasks
 *
 * @param arg Shared data passed to the thread
 * @return void* Thread return value
 */
typedef void *(*tg_task)(void *);


/**
 * @brief Initialization function signature
 *
 * Optional function to create or prepare shared data
 * before thread group creation
 *
 * @return void* Initialized shared data
 */
typedef void *(*tg_init)(void);

/**
 * @brief Cleanup function signature
 *
 * Function called after all threads in a group complete
 *
 * @param arg Shared data to be cleaned up
 * @return sdb_errno Success or error code
 */
typedef sdb_errno (*tg_cleanup)(void *);

typedef struct tg_manager tg_manager;


/**
 * @brief Thread Group Structure
 *
 * Represents a collection of threads sharing common resources
 * and managed as a single unit
 */
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


/**
 * @brief Thread Group Manager Structure
 *
 * Coordinates multiple thread groups, providing centralized
 * lifecycle management and synchronization
 */
struct tg_manager
{
    tg_group **Groups;

    u64 GroupCount;
    u64 CompletedCount;

    sdb_mutex Mutex;
    sdb_cond  Cond;
};


/**
 * @brief Create a Thread Group Manager
 *
 * Initializes a manager for coordinating multiple thread groups
 *
 * @param Groups Array of thread groups to manage
 * @param GroupCount Number of thread groups
 * @param A Optional memory arena for allocation
 * @return tg_manager* Initialized manager or NULL
 */
tg_manager *TgCreateManager(tg_group **Groups, u64 GroupCount, sdb_arena *A);


/**
 * @brief Destroy Thread Group Manager
 *
 * Releases resources associated with the manager
 *
 * @param M Manager to destroy
 */
void TgDestroyManager(tg_manager *M);

/**
 * @brief Create a Thread Group
 *
 * Configures a group of threads with shared resources
 * and optional initialization/cleanup functions
 *
 * @param GroupId Unique group identifier
 * @param ThreadCount Number of threads in the group
 * @param SharedData Optional pre-initialized shared data
 * @param Init Optional initialization function
 * @param Tasks Array of thread task functions
 * @param Cleanup Optional cleanup function
 * @param A Optional memory arena for allocation
 * @return tg_group* Initialized thread group or NULL
 */
tg_group *TgCreateGroup(u64 GroupId, u64 ThreadCount, void *SharedData, tg_init Init,
                        tg_task *Tasks, tg_cleanup Cleanup, sdb_arena *A);
/**
 * @brief Start All Threads in a Thread Group
 *
 * Initiates execution of all threads in the group
 * Launches a monitor thread to track group completion
 *
 * @param Group Thread group to start
 * @return sdb_errno Success or error code
 */
sdb_errno TgStartGroup(tg_group *Group);

/**
 * @brief Start All Thread Groups in Manager
 *
 * Initiates execution of all thread groups managed
 *
 * @param Manager Thread group manager
 * @return sdb_errno Success or error code
 */
sdb_errno TgManagerStartAll(tg_manager *Manager);

/**
 * @brief Wait for Completion of All Thread Groups
 *
 * Blocks until all thread groups complete or shutdown is requested
 *
 * @param Manager Thread group manager
 */
void TgManagerWaitForAll(tg_manager *Manager);

#endif
