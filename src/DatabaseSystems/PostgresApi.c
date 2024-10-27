#include <src/Sdb.h>
SDB_LOG_REGISTER(PostgresApi);
SDB_THREAD_ARENAS_EXTERN(Postgres);

#include <src/DatabaseSystems/DatabaseInitializer.h>
#include <src/DatabaseSystems/DatabaseSystems.h>
#include <src/DatabaseSystems/Postgres.h>

#if 0
sdb_errno
PgInit(database_api *Pg)
{
    PgInitThreadArenas();
    sdb_arena *Scratch1 = SdbArenaBootstrap(&Pg->Arena, NULL, SdbKibiByte(512));
    sdb_arena *Scratch2 = SdbArenaBootstrap(&Pg->Arena, NULL, SdbKibiByte(512));
    SdbThreadArenasAdd(Scratch1);
    SdbThreadArenasAdd(Scratch2);

    u64            ArenaF5  = SdbArenaGetPos(&Pg->Arena);
    sdb_file_data *ConfFile = SdbLoadFileIntoMemory(POSTGRES_CONF_FS_PATH, &Pg->Arena);
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

    postgres_ctx *PgCtx  = SdbPushStruct(&Pg->Arena, postgres_ctx);
    PgCtx->InsertBufSize = SdbKibiByte(4);
    PgCtx->InsertBuf     = SdbPushArray(&Pg->Arena, u8, PgCtx->InsertBufSize);
    PgCtx->DbConn        = Conn;
    Pg->Ctx              = PgCtx;

    cJSON *SchemaConf = DbInitGetConfFromFile("./configs/sensor_schemas.json", &Pg->Arena);
    if(ProcessSchemaConfig(Pg, SchemaConf, NULL, NULL) != 0) {
        cJSON_Delete(SchemaConf);
        return -1;
    }
    cJSON_Delete(SchemaConf);

    return 0;
}

sdb_errno
PgRun(database_api *Pg)
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
PgFinalize(database_api *Pg)
{
    PQfinish(PG_CTX(Pg)->DbConn);
    SdbArenaClear(&Pg->Arena);
    return 0;
}
#else
// NOTE(ingar): Since development uses the test versions of these api functions, these stub
// functions are used during development to avoid having to update them constantly
// whenever changes are made
sdb_errno
PgInit(database_api *Pg)
{
    return 0;
}
sdb_errno
PgRun(database_api *Pg)
{
    return 0;
}
sdb_errno
PgFinalize(database_api *Pg)
{
    return 0;
}
#endif
