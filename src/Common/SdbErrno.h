#ifndef SDB_ERRNO_H
#define SDB_ERRNO_H

#include <errno.h>

typedef enum
{
    SDBE_SUCCESS = 0,
    SDBE_ERR     = 1,

    SDBE_DBS_UNAVAIL = 2,

} sdb_errno;

#endif
