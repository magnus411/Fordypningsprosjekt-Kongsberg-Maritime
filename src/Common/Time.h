#ifndef SDB_TIME_H
#define SDB_TIME_H

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

sdb_errno SdbTimeNow(struct timespec *Timespec);
void      SdbTimeAdd(struct timespec *Timespec, sdb_timediff Delta);
sdb_errno SdbTimespecAbsolute(struct timespec *Timespec, sdb_timediff Delta);
void      SdbTimespec(struct timespec *Timespec, sdb_timediff Delta);
void      SdbTimeval(struct timeval *Timeval, sdb_timediff Delta);
bool      SdbTimeoutExpired(struct timespec *Timeout, struct timespec *Now);
sdb_errno SdbSleep(sdb_timediff Delta);

SDB_END_EXTERN_C

#endif
