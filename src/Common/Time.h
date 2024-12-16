#ifndef SDB_TIME_H
#define SDB_TIME_H


/**
 * @file Time.h
 * @brief Lightweight Time Manipulation Utilities
 *
 * Provides a set of time-related macros,
 * conversions, and utility functions for high-precision
 * time management.
 *
 */


#include <limits.h>
#include <time.h>

#include <src/Sdb.h>

SDB_BEGIN_EXTERN_C

#define SDB_TIME_BASE_NS_ (1)

typedef u64 sdb_timediff;

#define SDB_TIME_EQ(t1, t2) (t1 == t2)

#define SDB_TIME_MAX ((sdb_timediff){ UINT64_MAX })

#define SDB_TIME_S(s)     ((sdb_timediff){ s * 1e9 * SDB_TIME_BASE_NS_ })
#define SDB_TIME_TO_S(ns) ((u64)(ns / (1e9 * SDB_TIME_BASE_NS_)))

#define SDB_TIME_MS(ms)    ((sdb_timediff){ ms * 1e6 * SDB_TIME_BASE_NS_ })
#define SDB_TIME_TO_MS(ns) ((u64)(ns / (1e6 * SDB_TIME_BASE_NS_)))

#define SDB_TIME_US(us)    ((sdb_timediff){ us * 1e3 * SDB_TIME_BASE_NS_ })
#define SDB_TIME_TO_US(ns) ((uint64_t)(ns / (1e3 * SDB_TIME_BASE_NS_)))

#define SDB_TIME_NS(ns)    ((sdb_timediff){ ns * 1 * SDB_TIME_BASE_NS_ })
#define SDB_TIME_TO_NS(ns) ((uint64_t)(ns / (1 * SDB_TIME_BASE_NS_)))

#define SDB_TIMEOUT_NONE (SDB_TIME_NS(0))
#define SDB_TIMEOUT_MAX  (SDB_TIME_MAX)


/**
 * @brief Get Current Real-Time Timestamp
 *
 * Retrieves the current system time using CLOCK_REALTIME
 *
 * @param Timespec Pointer to timespec structure to store timestamp
 * @return sdb_errno Success or error code
 */
sdb_errno SdbTimeNow(struct timespec *Timespec);


/**
 * @brief Get Current Monotonic Timestamp
 *
 * Retrieves a monotonically increasing timestamp
 *
 * @param TimeStamp Pointer to timespec structure to store timestamp
 * @return sdb_errno Success or error code
 */
sdb_errno SdbTimeMonotonic(struct timespec *TimeStamp);

/**
 * @brief Calculate Time Difference Between Timestamps
 *
 * Computes the difference between two timestamps
 *
 * @param StartTime Beginning timestamp
 * @param EndTime Ending timestamp
 * @param Diff Optional pointer to store time difference
 * @return sdb_errno Success or error code
 */
sdb_errno SdbTimePrintSpecDiffWT(const struct timespec *StartTime, const struct timespec *EndTime,
                                 struct timespec *Diff);


/**
 * @brief Add Time Difference to Timestamp
 *
 * Modifies a timestamp by adding a time difference
 *
 * @param Timespec Timestamp to modify
 * @param Delta Time difference to add
 */
void SdbTimeAdd(struct timespec *Timespec, sdb_timediff Delta);

/**
 * @brief Create Absolute Timeout Timestamp
 *
 * Generates a future timestamp based on current time and delta
 *
 * @param Timespec Pointer to store absolute timestamp
 * @param Delta Time difference from now
 * @return sdb_errno Success or error code
 */
sdb_errno SdbTimespecAbsolute(struct timespec *Timespec, sdb_timediff Delta);

/**
 * @brief Convert Time Difference to Timespec
 *
 * Converts a time difference to a timespec structure
 *
 * @param Timespec Pointer to store timespec
 * @param Delta Time difference to convert
 */
void SdbTimespec(struct timespec *Timespec, sdb_timediff Delta);

/**
 * @brief Convert Time Difference to Timeval
 *
 * Converts a time difference to a timeval structure
 *
 * @param Timeval Pointer to store timeval
 * @param Delta Time difference to convert
 */
void SdbTimeval(struct timeval *Timeval, sdb_timediff Delta);

/**
 * @brief Check if Timeout Has Expired
 *
 * Compares current time against a timeout timestamp
 *
 * @param Timeout Timeout timestamp
 * @param Now Current timestamp
 * @return bool True if timeout has expired
 */
bool SdbTimeoutExpired(struct timespec *Timeout, struct timespec *Now);

/**
 * @brief Sleep for Specified Time
 *
 * Pauses execution for a given time difference
 *
 * @param Delta Time to sleep
 * @return sdb_errno Success or error code
 */
sdb_errno SdbSleep(sdb_timediff Delta);

SDB_END_EXTERN_C

#endif
