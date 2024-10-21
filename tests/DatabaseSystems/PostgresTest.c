#include <libpq-fe.h>

#include <src/Sdb.h>

SDB_LOG_REGISTER(PostgresTest);

#include <src/DatabaseSystems/DatabaseInitializer.h>
#include <src/DatabaseSystems/Postgres.h>
#include <src/Libs/cJSON/cJSON.h>
#include <tests/DatabaseSystems/PostgresTest.h>

void
TestBinaryInsert(PGconn *DbConnection, const char *TableName, u64 TableNameLen)
{
    power_shaft_sensor_data SensorData[] = {
        {.Id          = 0,
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
         .SensorId    = "SHAFT_SENSOR_01"},

        {.Id          = 1,
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
         .SensorId    = "SHAFT_SENSOR_01"},

        {.Id          = 2,
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
         .SensorId    = "SHAFT_SENSOR_01"}
    };

    for(int i = 0; i < 3; ++i) {
        SdbLogDebug("Inserting sensor %d into database", i);
        InsertSensorData(DbConnection, TableName, TableNameLen, (u8 *)&SensorData[i],
                         sizeof(power_shaft_sensor_data));
    }
}

sdb_errno
PgInitTest(database_api *Pg)
{
    u64            ArenaF5  = SdbArenaGetPos(&Pg->Arena);
    sdb_file_data *ConfFile = SdbLoadFileIntoMemory(POSTGRES_CONF_FS_PATH, &Pg->Arena);
    // TODO(ingar): Make pg config a json file
    if(ConfFile == NULL) {
        SdbLogError("Failed to open config file");
        return -1;
    }

    const char *ConnInfo = (const char *)ConfFile->Data;
    SdbLogInfo("Attempting to connect to Postgres database using: %s", ConnInfo);

    PGconn *Conn = PQconnectdb(ConnInfo);
    if(PQstatus(Conn) != CONNECTION_OK) {
        SdbLogError("Connection to database failed. PQ error:\n%s", PQerrorMessage(Conn));
        PQfinish(Conn);
        SdbArenaSeek(&Pg->Arena, ArenaF5);
        return -1;
    } else {
        SdbLogInfo("Connected!");
    }
    SdbArenaSeek(&Pg->Arena, ArenaF5);

    cJSON *SchemaConf = DbInitGetConfFromFile("configs/shaft_power.json", &Pg->Arena);
    if(CreateTablesFromSchemaConf(Conn, SchemaConf, &Pg->Arena) != 0) {
        cJSON_Delete(SchemaConf);
        return -1;
    }
    cJSON_Delete(SchemaConf);

    postgres_ctx *PgCtx  = SdbPushStruct(&Pg->Arena, postgres_ctx);
    PgCtx->InsertBufSize = SdbKibiByte(4);
    PgCtx->InsertBuf     = SdbPushArray(&Pg->Arena, u8, PgCtx->InsertBufSize);
    PgCtx->DbConn        = Conn;
    Pg->Ctx              = PgCtx;

    return 0;
}

sdb_errno
PgRunTest(database_api *Pg)
{
    while(true) {
        ssize_t Ret = SdPipeRead(&Pg->SdPipe, 0, PG_CTX(Pg)->InsertBuf, sizeof(queue_item));
        if(Ret > 0) {
            SdbLogDebug("Succesfully read %zd from pipe!", Ret);
        } else {
            SdbLogError("Failed to read from pipe");
        }
    }
    return 0;
}

sdb_errno
PgFinalizeTest(database_api *Pg)
{
    PQfinish(PG_CTX(Pg)->DbConn);
    SdbArenaClear(&Pg->Arena);
    return 0;
}
