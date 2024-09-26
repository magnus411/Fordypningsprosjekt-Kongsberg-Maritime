#define SDB_LOG_LEVEL 4
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
    int              ShaftColCount;
    const char      *PowerShaftSensorTableName = "power_shaft_sensor";
    u64              PSSTableNameLen           = 18;
    pq_col_metadata *Metadata
        = GetTableMetadata(Connection, PowerShaftSensorTableName, PSSTableNameLen, &ShaftColCount);
    for(int i = 0; i < ShaftColCount; ++i) {
        PrintColumnMetadata(&Metadata[i]);
    }

    TestBinaryInsert(Connection, PowerShaftSensorTableName, PSSTableNameLen);

    PQfinish(Connection);

    return 0;
}
