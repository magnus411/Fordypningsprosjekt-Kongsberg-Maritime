#ifndef DATA_STORAGE_H
#define DATA_STORAGE_H

#include <libpq-fe.h>
#include <src/Sdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef int sdb_errno;

sdb_errno InitializeRLEMetaDataStorage(PGconn *Conn);
sdb_errno StoreRLEMetaData(const char *Start, const char *End, size_t RunLength, PGconn *Conn);
sdb_errno StoreRLEMetaDataWithTimespecs(const struct timespec *Start, const struct timespec *End,
                                        size_t RunLength, PGconn *Conn);
sdb_errno InitializeFullMetaDataStorage(PGconn *Conn);
sdb_errno StoreMetaData(struct timespec *TimeStamp, PGconn *Conn);

#endif // DATA_STORAGE_H
