#include <libpq-fe.h>

#define SDB_LOG_LEVEL 4
#include <Sdb.h>

SDB_LOG_REGISTER(TestMain);

// NOTE(ingar): <> includes are for the actual program, "" are for exported test functions
#include <database_systems/Postgres.h>

#include "../comm_protocols/ModbusTest.h"
#include "Postgres.h"

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
