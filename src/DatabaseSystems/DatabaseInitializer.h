#ifndef DATABASE_INITIALIZER_H
#define DATABASE_INITIALIZER_H

#include <src/Sdb.h>

SDB_BEGIN_EXTERN_C

#include <src/Libs/cJSON/cJSON.h>

cJSON *DbInitGetConfFromFile(const char *Filename, sdb_arena *A);

SDB_END_EXTERN_C

#endif // DATABASE_INITIALIZER_H
