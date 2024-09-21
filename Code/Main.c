#define SDB_LOG_LEVEL    4
#define SDB_LOG_BUF_SIZE 1024
#include "Sdb.h"

#include "Postgres.h"
#include "DatabaseModule.h"

SDB_LOG_REGISTER(Main);

#include <stdlib.h>
#include <libpq-fe.h>

typedef struct
{
    int   Id;
    char *Name;
    int   SampleRate;
    char *Variables;
} sensor_schema;

sdb_internal void
GetSensorSchemasFromDb(PGconn *Connection, sdb_arena *Arena)
{

    sensor_schema *Schemas     = NULL;
    int            SchemaCount = 0;

    const char *Query      = "SELECT id, name, sample_rate, variables FROM sensor_schemas";
    PGresult   *ExecResult = PQexec(Connection, Query);

    if(PQresultStatus(ExecResult) != PGRES_TUPLES_OK) {
        SdbLogError("Failed to execute query:\n%s", PQerrorMessage(Connection));
        PQclear(ExecResult);
        return;
    }

    SchemaCount = PQntuples(ExecResult);
    Schemas     = SdbPushArray(Arena, sensor_schema, SchemaCount);

    for(int i = 0; i < SchemaCount; ++i) {
        Schemas[i].Id         = atoi(PQgetvalue(ExecResult, i, 0));
        Schemas[i].Name       = SdbStrdup(PQgetvalue(ExecResult, i, 1), Arena);
        Schemas[i].SampleRate = atoi(PQgetvalue(ExecResult, i, 2));
        Schemas[i].Variables  = SdbStrdup(PQgetvalue(ExecResult, i, 3), Arena);
    }

    for(int i = 0; i < SchemaCount; i++) {
        SdbLogDebug("Template ID: %d, Name: %s, Sample Rate: %d, Variables: %s\n", Schemas[i].Id,
                    Schemas[i].Name, Schemas[i].SampleRate, Schemas[i].Variables);
    }

    PQclear(ExecResult);
}

void
DiagnoseConnectionAndTable(PGconn *DbConn, const char *TableName)
{
    printf("Connected to database: %s\n", PQdb(DbConn));
    printf("User: %s\n", PQuser(DbConn));

    // Check if the table exists
    const char *TableCheckQuery
        = "SELECT EXISTS (SELECT 1 FROM pg_tables WHERE schemaname = 'public' AND tablename = $1)";
    const char *ParamValues[1] = { TableName };
    PGresult   *TableCheckResult
        = PQexecParams(DbConn, TableCheckQuery, 1, NULL, ParamValues, NULL, NULL, 0);

    if(PQresultStatus(TableCheckResult) == PGRES_TUPLES_OK && PQntuples(TableCheckResult) > 0) {
        printf("Table '%s' exists: %s\n", TableName,
               strcmp(PQgetvalue(TableCheckResult, 0, 0), "t") == 0 ? "Yes" : "No");
    } else {
        printf("Error checking table existence: %s\n", PQerrorMessage(DbConn));
    }
    PQclear(TableCheckResult);

    // List all tables in public schema
    const char *ListTablesQuery  = "SELECT tablename FROM pg_tables WHERE schemaname = 'public'";
    PGresult   *ListTablesResult = PQexec(DbConn, ListTablesQuery);

    if(PQresultStatus(ListTablesResult) == PGRES_TUPLES_OK) {
        printf("Tables in public schema:\n");
        for(int i = 0; i < PQntuples(ListTablesResult); i++) {
            printf("  %s\n", PQgetvalue(ListTablesResult, i, 0));
        }
    } else {
        printf("Error listing tables: %s\n", PQerrorMessage(DbConn));
    }
    PQclear(ListTablesResult);
}

int
main(int ArgCount, char **ArgV)
{
    if(ArgCount <= 1) {
        SdbLogError("Please provide the path to the configuration.sdb file!\nRelative paths start "
                    "from where YOU launch the executable from.");
        return 1;
    }

    sdb_arena MainArena;
    u64       ArenaMemorySize = SdbMebiByte(128);
    u8       *ArenaMemory     = malloc(ArenaMemorySize);
    SdbArenaInit(&MainArena, ArenaMemory, ArenaMemorySize);

    const char    *ConfigFilePath = ArgV[1];
    sdb_file_data *ConfigFile     = SdbLoadFileIntoMemory(ConfigFilePath, &MainArena);
    if(NULL == ConfigFile) {
        SdbLogError("Failed to open file!");
        return 1;
    }

    // TODO(ingar): Make parser for config file
    const char *ConnectionInfo = (const char *)ConfigFile->Data;
    SdbLogInfo("Attempting to connect to database using:\n%s", ConnectionInfo);

    PGconn *Connection = PQconnectdb(ConnectionInfo);
    if(PQstatus(Connection) != CONNECTION_OK) {
        SdbLogError("Connection to database failed. Libpq error:\n%s", PQerrorMessage(Connection));
        PQfinish(Connection);
        return 1;
    }

    SdbLogInfo("Connected!");
    DiagnoseConnectionAndTable(Connection, "power_shaft_sensor");
    //    GetSensorSchemasFromDb(Connection, &MainArena);
    int   ShaftColCount;
    char *PowerShaftMetadata
        = GetSqlQueryFromFile("DebugStuff/column-metadata-query.sql", &MainArena);

    // TestGetMetadata(Connection, PowerShaftMetadata, &ShaftColCount);
    //  TestBinaryInsert(Connection);

    PQfinish(Connection);

    return 0;
}
