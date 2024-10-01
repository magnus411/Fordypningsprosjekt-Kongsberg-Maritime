#include <libpq-fe.h>

#define SDB_LOG_LEVEL 4
#include <Sdb.h>

SDB_LOG_REGISTER(PostgresTest);

// NOTE(ingar): <> includes are for the actual program, "" are for exported test functions
#include <database_systems/Postgres.h>

#include "PostgresTest.h"

typedef struct __attribute__((packed))
{
    i64    Id;
    i64    Timestamp;    // TIMESTAMPTZ
    int    Rpm;          // INTEGER
    double Torque;       // DOUBLE PRECISION
    double Temperature;  // DOUBLE PRECISION
    double Vibration_x;  // DOUBLE PRECISION
    double Vibration_y;  // DOUBLE PRECISION
    double Vibration_z;  // DOUBLE PRECISION
    double Strain;       // DOUBLE PRECISION
    double PowerOutput;  // DOUBLE PRECISION
    double Efficiency;   // DOUBLE PRECISION
    double ShaftAngle;   // DOUBLE PRECISION
    char   SensorId[51]; // VARCHAR(50)
} power_shaft_sensor_data;

typedef struct
{
    int   Id;
    char *Name;
    int   SampleRate;
    char *Variables;
} sensor_schema;

void
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
TestBinaryInsert(PGconn *DbConnection, const char *TableName, u64 TableNameLen)
{
    power_shaft_sensor_data SensorData[] = {
        { .Id          = 0,
         .Timestamp   = 0,
         .Rpm         = 3000,
         .Torque      = 150.5,
         .Temperature = 85.2,
         .Vibration_x = 0.05,
         .Vibration_y = 0.03,
         .Vibration_z = 0.04,
         .Strain      = 0.002,
         .PowerOutput = 470.3,
         .Efficiency  = 0.92,
         .ShaftAngle  = 45.0,
         .SensorId    = "SHAFT_SENSOR_01" },

        { .Id          = 1,
         .Timestamp   = 60,
         .Rpm         = 3050,
         .Torque      = 155.2,
         .Temperature = 86.5,
         .Vibration_x = 0.06,
         .Vibration_y = 0.04,
         .Vibration_z = 0.05,
         .Strain      = 0.0022,
         .PowerOutput = 480.1,
         .Efficiency  = 0.91,
         .ShaftAngle  = 46.5,
         .SensorId    = "SHAFT_SENSOR_01" },

        { .Id          = 2,
         .Timestamp   = 120,
         .Rpm         = 2980,
         .Torque      = 148.9,
         .Temperature = 84.8,
         .Vibration_x = 0.04,
         .Vibration_y = 0.03,
         .Vibration_z = 0.03,
         .Strain      = 0.0019,
         .PowerOutput = 465.7,
         .Efficiency  = 0.93,
         .ShaftAngle  = 44.2,
         .SensorId    = "SHAFT_SENSOR_01" }
    };

    for(int i = 0; i < 3; ++i) {
        SdbLogDebug("Inserting sensor %d into database", i);
        InsertSensorData(DbConnection, TableName, TableNameLen, (u8 *)&SensorData[i],
                         sizeof(power_shaft_sensor_data));
    }
}

void
RunPostgresTest(const char *ConfigFilePath)
{
    SdbLogInfo("Running Postgres Test...");

    sdb_arena MainArena;
    u64       ArenaMemorySize = SdbMebiByte(128);
    u8       *ArenaMemory     = malloc(ArenaMemorySize);
    SdbArenaInit(&MainArena, ArenaMemory, ArenaMemorySize);

    sdb_file_data *ConfigFile = SdbLoadFileIntoMemory(ConfigFilePath, &MainArena);
    if(NULL == ConfigFile) {
        SdbLogError("Failed to open config file!");
        return;
    }

    const char *ConnectionInfo = (const char *)ConfigFile->Data;
    SdbLogInfo("Attempting to connect to database using:\n%s", ConnectionInfo);

    PGconn *Connection = PQconnectdb(ConnectionInfo);
    if(PQstatus(Connection) != CONNECTION_OK) {
        SdbLogError("Connection to database failed: %s", PQerrorMessage(Connection));
        PQfinish(Connection);
        return;
    }

    SdbLogInfo("Connected!");
    DiagnoseConnectionAndTable(Connection, "power_shaft_sensor");

    PQfinish(Connection);
    SdbLogInfo("Connection closed. Goodbye!");
}
