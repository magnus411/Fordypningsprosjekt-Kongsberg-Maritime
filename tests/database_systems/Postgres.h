#include <libpq-fe.h>
#include <SdbExtern.h>

void TestBinaryInsert(PGconn *DbConnection, const char *TableName, u64 TableNameLen);
void GetSensorSchemasFromDb(PGconn *Connection, sdb_arena *Arena);
