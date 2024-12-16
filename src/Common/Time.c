#include <time.h>

#include <src/Sdb.h>


/**
 * @file Time.c
 * @brief Implementation of Time Manipulation Utilities
 *
 */

#include <src/Common/Time.h>

sdb_errno
SdbTimeNow(struct timespec *Timespec)
{
    if(clock_gettime(CLOCK_REALTIME, Timespec) != 0) {
        return -errno;
    }

    return 0;
}

sdb_errno
SdbTimeMonotonic(struct timespec *TimeStamp)
{
    if(clock_gettime(CLOCK_MONOTONIC, TimeStamp) != 0) {
        return -1;
    }

    return 0;
}

void
SdbTimeAdd(struct timespec *Timespec, sdb_timediff Delta)
{
    Timespec->tv_sec += SDB_TIME_TO_S(Delta);
    Timespec->tv_nsec += SDB_TIME_TO_NS(Delta) % (u64)1e9;

    if(Timespec->tv_nsec >= 1e9) {
        Timespec->tv_sec += 1;
        Timespec->tv_nsec %= (u64)1e9;
    }
}

sdb_errno
SdbTimespecAbsolute(struct timespec *Timespec, sdb_timediff Delta)
{
    sdb_errno Ret = SdbTimeNow(Timespec);
    if(Ret != 0) {
        return Ret;
    }

    SdbTimeAdd(Timespec, Delta);

    return Ret;
}

sdb_errno
SdbTimePrintSpecDiffWT(const struct timespec *StartTime, const struct timespec *EndTime,
                       struct timespec *Diff)
{
    struct timespec TimeDiff;

    // Handle case where end nanoseconds is less than start nanoseconds
    if(EndTime->tv_nsec < StartTime->tv_nsec) {
        TimeDiff.tv_sec  = EndTime->tv_sec - StartTime->tv_sec - 1;
        TimeDiff.tv_nsec = 1000000000L + EndTime->tv_nsec - StartTime->tv_nsec;
    } else {
        TimeDiff.tv_sec  = EndTime->tv_sec - StartTime->tv_sec;
        TimeDiff.tv_nsec = EndTime->tv_nsec - StartTime->tv_nsec;
    }

    if(Diff == NULL) {
        SdbPrintfDebug("Time difference: %ld.%09ld seconds\n", TimeDiff.tv_sec, TimeDiff.tv_nsec);
    } else {
        SdbMemcpy(Diff, &TimeDiff, sizeof(TimeDiff));
    }

    return 0;
}

void
SdbTimespec(struct timespec *Timespec, sdb_timediff Delta)
{
    Timespec->tv_sec  = SDB_TIME_TO_S(Delta);
    Timespec->tv_nsec = SDB_TIME_TO_NS(Delta) % (u64)1e9;
}

void
SdbTimeval(struct timeval *Timeval, sdb_timediff Delta)
{
    Timeval->tv_sec  = SDB_TIME_TO_S(Delta);
    Timeval->tv_usec = SDB_TIME_TO_US(Delta) & (u64)1e6;
}

bool
SdbTimeoutExpired(struct timespec *Timeout, struct timespec *Now)
{
    i64 TimeoutNs = Timeout->tv_sec * 1e9 + Timeout->tv_nsec;
    i64 NowNs     = Now->tv_sec * 1e9 + Now->tv_nsec;

    return NowNs > TimeoutNs;
}

sdb_errno
SdbSleep(sdb_timediff Delta)
{
    struct timespec Timespec;
    SdbTimespec(&Timespec, Delta);

    sdb_errno Ret = nanosleep(&Timespec, NULL);
    if(Ret != 0) {
        return -errno;
    }

    return Ret;
}
