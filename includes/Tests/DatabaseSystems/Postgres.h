#ifndef POSTGRES_TEST_H
#define POSTGRES_TEST_H
#include <Sdb.h>
#include <libpq-fe.h>
void RunPostgresTest(const char *ConfigFilePath);
void TestBinaryInsert(PGconn *DbConnection, const char *TableName, u64 TableNameLen);
void GetSensorSchemasFromDb(PGconn *Connection, sdb_arena *Arena);

#endif
