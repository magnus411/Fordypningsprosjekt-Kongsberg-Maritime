#include <libpq-fe.h>

#define SDB_LOG_LEVEL 4
#include "Sdb.h"

#include "databases/Postgres.h"
#include "../src/Postgres.h"
#include "../src/DatabaseModule.h"

SDB_LOG_REGISTER(TestMain);

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
