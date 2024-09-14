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
GetSensorSchemasFromDb(PGconn *Connection, sensor_schema **Schemas, int *TemplateCount,
                       isa_arena *Arena)
{

    const char *Query      = "SELECT id, name, sample_rate, variables FROM sensortemplates";
    PGresult   *ExecResult = PQexec(Connection, Query);

    if(PQresultStatus(ExecResult) != PGRES_TUPLES_OK)
    {
        IsaLogError("Failed to execute query: %s", PQerrorMessage(Connection));
        PQclear(ExecResult);
        return;
    }

    *TemplateCount = PQntuples(ExecResult);
    *Schemas       = IsaPushArray(Arena, sensor_schema, *TemplateCount);

    for(int i = 0; i < *TemplateCount; i++)
    {
        (*Schemas)[i].Id         = atoi(PQgetvalue(ExecResult, i, 0));
        (*Schemas)[i].Name       = strdup(PQgetvalue(ExecResult, i, 1));
        (*Schemas)[i].SampleRate = atoi(PQgetvalue(ExecResult, i, 2));
        (*Schemas)[i].Variables  = strdup(PQgetvalue(ExecResult, i, 3));
    }

    PQclear(ExecResult);
}

isa_internal void
FreeTemplates(sensor_schema *Schemas, int TemplateCount)
{
    for(int i = 0; i < TemplateCount; i++)
    {
        free(Schemas[i].Name);
        free(Schemas[i].Variables);
    }
}

int
main(void)
{
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

    if(PQstatus(Connection) != CONNECTION_OK)
    {
        IsaLogError("Connection to database failed: %s", PQerrorMessage(Connection));
        PQfinish(Connection);
        exit(EXIT_FAILURE);
    }
    else
    {
        IsaLogDebug("Connected!");
    }

    sensor_schema *Templates     = NULL;
    int            TemplateCount = 0;
    GetSensorSchemasFromDb(Connection, &Templates, &TemplateCount, &MainArena);

    for(int i = 0; i < TemplateCount; i++)
    {
        IsaLogDebug("Template ID: %d, Name: %s, Sample Rate: %d, Variables: %s\n", Templates[i].Id,
                    Templates[i].Name, Templates[i].SampleRate, Templates[i].Variables);
    }

    FreeTemplates(Templates, TemplateCount);
    PQfinish(Connection);

    return 0;
}
