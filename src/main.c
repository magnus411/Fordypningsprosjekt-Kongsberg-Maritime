#define ISA_LOG_LEVEL    4
#define ISA_LOG_BUF_SIZE 256
#include "isa.h"

ISA_LOG_REGISTER(main);

#include <stdlib.h>
#include <libpq-fe.h>

typedef struct
{
    int   Id;
    char *Name;
    int   SampleRate;
    char *Variables;
} sensor_schema;

isa_internal void
GetSensorSchemasFromDb(PGconn *Connection, sensor_schema **Schemas, int *SchemaCount,
                       isa_arena *Arena)
{

    const char *Query      = "SELECT id, name, sample_rate, variables FROM sensor_schemas";
    PGresult   *ExecResult = PQexec(Connection, Query);

    if(PQresultStatus(ExecResult) != PGRES_TUPLES_OK)
    {
        IsaLogError("Failed to execute query:\n%s", PQerrorMessage(Connection));
        PQclear(ExecResult);
        return;
    }

    *SchemaCount = PQntuples(ExecResult);
    *Schemas     = IsaPushArray(Arena, sensor_schema, *SchemaCount);

    for(int i = 0; i < *SchemaCount; ++i)
    {
        (*Schemas)[i].Id         = atoi(PQgetvalue(ExecResult, i, 0));
        (*Schemas)[i].Name       = IsaStrdup(PQgetvalue(ExecResult, i, 1), Arena);
        (*Schemas)[i].SampleRate = atoi(PQgetvalue(ExecResult, i, 2));
        (*Schemas)[i].Variables  = IsaStrdup(PQgetvalue(ExecResult, i, 3), Arena);
    }

    PQclear(ExecResult);
}

int
main(int ArgCount, char **ArgV)
{
    if(ArgCount <= 1)
    {
        IsaLogError("Please provide the path to the configuration.sdb file!\nRelative paths start "
                    "from where YOU launch the executable from.");
        return 1;
    }

    isa_arena MainArena;
    u64       ArenaMemorySize = IsaMebiByte(128);
    u8       *ArenaMemory     = malloc(ArenaMemorySize);
    IsaArenaInit(&MainArena, ArenaMemory, ArenaMemorySize);

    const char    *ConfigFilePath = ArgV[1];
    isa_file_data *ConfigFile     = IsaLoadFileIntoMemory(ConfigFilePath, &MainArena);
    if(NULL == ConfigFile)
    {
        IsaLogError("Failed to open file!");
        return 1;
    }

    // TODO(ingar): Make parser for config file
    const char *ConnectionInfo = (const char *)ConfigFile->Data;
    IsaLogInfo("Attempting to connect to database using:\n%s", ConnectionInfo);

    PGconn *Connection = PQconnectdb(ConnectionInfo);
    if(PQstatus(Connection) != CONNECTION_OK)
    {
        IsaLogError("Connection to database failed. Libpq error:\n%s", PQerrorMessage(Connection));
        PQfinish(Connection);
        return 1;
    }

    IsaLogInfo("Connected!");

    sensor_schema *Schemas     = NULL;
    int            SchemaCount = 0;
    GetSensorSchemasFromDb(Connection, &Schemas, &SchemaCount, &MainArena);

    for(int i = 0; i < SchemaCount; i++)
    {
        IsaLogDebug("Template ID: %d, Name: %s, Sample Rate: %d, Variables: %s\n", Schemas[i].Id,
                    Schemas[i].Name, Schemas[i].SampleRate, Schemas[i].Variables);
    }

    PQfinish(Connection);

    return 0;
}
