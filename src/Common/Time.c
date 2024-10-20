#include <time.h>

#include <src/Sdb.h>

#include <src/Common/Time.h>

sdb_errno
SdbTimeNow(struct timespec *Timespec)
{
    if(clock_gettime(CLOCK_REALTIME, Timespec) != 0) {
        return -errno;
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

void
SdbTimespec(struct timespec *Timespec, sdb_timediff Delta)
{
    Timespec->tv_sec  = SDB_TIME_TO_S(Delta);
    Timespec->tv_nsec = SDB_TIME_TO_NS(Delta) % (uint64_t)1e9;
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
