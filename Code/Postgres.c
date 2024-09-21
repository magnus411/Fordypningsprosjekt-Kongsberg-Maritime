#include "SdbExtern.h"
#include "Postgres.h"

SDB_LOG_REGISTER(Postgres);

char *
GetSqlQueryFromFile(const char *FileName, sdb_arena *Arena)
{
    sdb_file_data *File = SdbLoadFileIntoMemory(FileName, Arena);
    return (char *)File->Data;
}
