#ifndef SDB_ERRNO_H
#define SDB_ERRNO_H

#include <errno.h>

typedef enum
{
    SDBE_SUCCESS = 0,
    SDBE_ERR     = 1,

    SDBE_DBS_UNAVAIL = 2,
    SDBE_CP_UNAVAIL  = 3,

    SDBE_CONN_CLOSED_SUCS = 4,
    SDBE_CONN_CLOSED_ERR  = 5,

} sdb_errno;

#endif
