#ifndef POSTGRES_TEST_H
#define POSTGRES_TEST_H
#include <libpq-fe.h>
#include <src/Sdb.h>
void RunPostgresTest(const char *ConfigFilePath);
void TestBinaryInsert(PGconn *DbConnection, const char *TableName, u64 TableNameLen);
void GetSensorSchemasFromDb(PGconn *Connection, sdb_arena *Arena);

#endif
