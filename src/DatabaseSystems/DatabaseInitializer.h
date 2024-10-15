#ifndef DATABASE_INITIALIZER_H
#define DATABASE_INITIALIZER_H

#include <src/Sdb.h>

#include <src/Libs/cJSON/cJSON.h>

// ccJSON **DbInitGetSchemasFromDb(PGconn *Conn);

cJSON *DbInitGetConfFromFile(const char *filename, sdb_arena *Arena);

#endif // DATABASE_INITIALIZER_H
