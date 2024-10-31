#include "src/Common/Time.h"
#include "tests/TestConstants.h"
#include <arpa/inet.h>
#include <libpq-fe.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>

#if defined(__linux__)
#include <endian.h>
#elif defined(__APPLE__)
#include <machine/endian.h>
#else
#error Unsupported platform
#endif

#include <src/Sdb.h>

SDB_LOG_REGISTER(Postgres);
SDB_THREAD_ARENAS_REGISTER(Postgres, 2);

#include <src/Common/CircularBuffer.h>
#include <src/Common/SensorDataPipe.h>
#include <src/Common/Thread.h>
#include <src/DatabaseSystems/DatabaseInitializer.h>
#include <src/DatabaseSystems/DatabaseSystems.h>
#include <src/DatabaseSystems/Postgres.h>
#include <src/Libs/cJSON/cJSON.h>

void
PgInitThreadArenas(void)
{
    SdbThreadArenasInit(Postgres);
}

void
DiagnoseConnectionAndTable(PGconn *DbConn, const char *TableName)
{
    SdbLogInfo("Connected to database: %s", PQdb(DbConn));
    SdbLogInfo("User: %s", PQuser(DbConn));

    // Check if the table exists
    const char *TableCheckQuery
        = "SELECT EXISTS (SELECT 1 FROM pg_tables WHERE schemaname = 'public' AND tablename = $1)";
    const char *ParamValues[1] = { TableName };
    PGresult   *TableCheckResult
        = PQexecParams(DbConn, TableCheckQuery, 1, NULL, ParamValues, NULL, NULL, 0);

    if(PQresultStatus(TableCheckResult) == PGRES_TUPLES_OK && PQntuples(TableCheckResult) > 0) {
        SdbLogInfo("Table '%s' exists: %s", TableName,
                   strcmp(PQgetvalue(TableCheckResult, 0, 0), "t") == 0 ? "Yes" : "No");
    } else {
        SdbLogError("Error checking table existence: %s", PQerrorMessage(DbConn));
    }
    PQclear(TableCheckResult);

    // List all tables in public schema
    const char *ListTablesQuery  = "SELECT tablename FROM pg_tables WHERE schemaname = 'public'";
    PGresult   *ListTablesResult = PQexec(DbConn, ListTablesQuery);

    if(PQresultStatus(ListTablesResult) == PGRES_TUPLES_OK) {
        SdbLogInfo("Tables in public schema:");
        for(int i = 0; i < PQntuples(ListTablesResult); i++) {
            SdbLogInfo("  %s", PQgetvalue(ListTablesResult, i, 0));
        }
    } else {
        SdbLogError("Error listing tables: %s", PQerrorMessage(DbConn));
    }
    PQclear(ListTablesResult);
}

void
PrintPGresult(const PGresult *Result)
{
    // NOTE(ingar): Since it's a printing function, using printf instead of logging functions is
    ExecStatusType Status = PQresultStatus(Result);
    SdbPrintfDebug("Result Status: %s\n", PQresStatus(Status));

    if(Status == PGRES_FATAL_ERROR || Status == PGRES_NONFATAL_ERROR) {
        SdbLogError("Error Message: %s", PQresultErrorMessage(Result));
    }

    int NumRows = PQntuples(Result);
    int NumCols = PQnfields(Result);
    SdbPrintfDebug("Number of rows: %d\n", NumRows);
    SdbPrintfDebug("Number of columns: %d\n", NumCols);

    SdbPrintfDebug("Columns:\n");
    for(int Col = 0; Col < NumCols; Col++) {
        SdbPrintfDebug("  %d: %s\n", Col, PQfname(Result, Col));
    }

    int RowsToPrint = NumRows < 5 ? NumRows : 5;
    SdbPrintfDebug("First %d rows:\n", RowsToPrint);
    for(int Row = 0; Row < RowsToPrint; Row++) {
        SdbPrintfDebug("Row %d:\n", Row);
        for(int Col = 0; Col < NumCols; Col++) {
            SdbPrintfDebug("  %s: %s\n", PQfname(Result, Col), PQgetvalue(Result, Row, Col));
        }
    }

    if(NumRows > 5) {
        SdbPrintfDebug("... (%d more rows)\n", NumRows - 5);
    }
}


pg_col_metadata *
GetTableMetadata(PGconn *DbConn, sdb_string TableName, int *ColCount, sdb_arena *A)
{
    sdb_arena        *Conflicts[1] = { A };
    sdb_scratch_arena Scratch      = SdbScratchGet(Conflicts, 1);

    sdb_string MetadataQuery = SdbStringMake(Scratch.Arena, NULL);
    SdbStringAppendFmt(MetadataQuery, PQ_TABLE_METADATA_QUERY_FMT, TableName);

    PGresult *Result = PQexec(DbConn, MetadataQuery);
    if(PQresultStatus(Result) != PGRES_TUPLES_OK) {
        SdbLogError("Metadata query failed: %s", PQerrorMessage(DbConn));
        PQclear(Result);
        SdbScratchRelease(Scratch);
        return NULL;
    }
    SdbScratchRelease(Scratch);

    int RowCount = PQntuples(Result);
    // NOTE(ingar): Each row in the result is the metadata for one column in the requested table
    *ColCount = RowCount;

    pg_col_metadata *MetadataArray = SdbPushArray(A, pg_col_metadata, RowCount);
    if(!MetadataArray) {
        SdbLogError("Memory allocation for PG metadata failed");
        PQclear(Result);
        return NULL;
    }


    for(int RowIndex = 0; RowIndex < RowCount; ++RowIndex) {
        pg_col_metadata *CurrentMetadata = &MetadataArray[RowIndex];

        CurrentMetadata->ColumnName   = SdbStringMake(A, PQgetvalue(Result, RowIndex, 0));
        CurrentMetadata->TypeOid      = (pg_oid)atoi(PQgetvalue(Result, RowIndex, 1));
        CurrentMetadata->TypeLength   = (i16)atoi(PQgetvalue(Result, RowIndex, 2));
        CurrentMetadata->TypeModifier = (i32)atoi(PQgetvalue(Result, RowIndex, 3));
        if(RowIndex > 0) {
            pg_col_metadata PrevMetadata = MetadataArray[RowIndex - 1];
            CurrentMetadata->Offset      = PrevMetadata.Offset + PrevMetadata.TypeLength;
        } else {
            CurrentMetadata->Offset = 0;
        }
    }

    PQclear(Result);

    return MetadataArray;
}


sdb_errno
PgPrepare(database_api *Pg)
{
    postgres_ctx     *PgCtx   = PG_CTX(Pg);
    sdb_scratch_arena Scratch = SdbScratchGet(NULL, 0);

    // TODO(ingar): Make pg config a json file??
    sdb_file_data *ConfFile = SdbLoadFileIntoMemory(POSTGRES_CONF_FS_PATH, Scratch.Arena);
    if(ConfFile == NULL) {
        SdbLogError("Failed to open config file");
        SdbScratchRelease(Scratch);
        return -1;
    }

    const char *ConnInfo = (const char *)ConfFile->Data;
    PgCtx->DbConn        = PQconnectdb(ConnInfo);

    if(PQstatus(PgCtx->DbConn) != CONNECTION_OK) {
        SdbLogError("PgCtx->DbConnection to database failed. PQ error:\n%s",
                    PQerrorMessage(PgCtx->DbConn));
        SdbScratchRelease(Scratch);
        PQfinish(PgCtx->DbConn);
        return -1;
    }


    cJSON *SchemaConf = DbInitGetConfFromFile("./configs/sensor_schemas.json", Scratch.Arena);
    if(!cJSON_IsObject(SchemaConf)) {
        goto JSON_err;
    }

    cJSON *SensorSchemaArray = cJSON_GetObjectItem(SchemaConf, "sensors");
    if(SensorSchemaArray == NULL || !cJSON_IsArray(SensorSchemaArray)) {
        goto JSON_err;
    }


    u64    SensorIdx    = 0;
    cJSON *SensorSchema = NULL;
    cJSON_ArrayForEach(SensorSchema, SensorSchemaArray)
    {
        if(!cJSON_IsObject(SensorSchema)) {
            goto JSON_err;
        }

        cJSON *SensorName = cJSON_GetObjectItem(SensorSchema, "name");
        if(SensorName == NULL || !cJSON_IsString(SensorName)) {
            goto JSON_err;
        }

        pg_table_info *Ti = PgCtx->TablesInfo[SensorIdx];

        Ti            = SdbPushStruct(&Pg->Arena, pg_table_info);
        Ti->TableName = SdbStringMake(&Pg->Arena, SensorName->valuestring);
        Ti->Statement = SdbStringDuplicate(&Pg->Arena, Ti->TableName);
        SdbStringAppendC(Ti->Statement, "_copy");


        sdb_string CreationQuery = SdbStringMake(Scratch.Arena, "CREATE TABLE IF NOT EXISTS ");
        SdbStringAppendC(CreationQuery, SensorName->valuestring);
        SdbStringAppendC(CreationQuery, "(\nid SERIAL PRIMARY KEY,\n");

        cJSON *SensorData = cJSON_GetObjectItem(SensorSchema, "data");
        if(SensorData == NULL || !cJSON_IsObject(SensorData)) {
            SdbScratchRelease(Scratch);
            goto JSON_err;
        }

        cJSON *DataAttribute = NULL;
        cJSON_ArrayForEach(DataAttribute, SensorData)
        {
            if(!cJSON_IsString(DataAttribute)) {
                SdbScratchRelease(Scratch);
                goto JSON_err;
            }

            SdbStringAppendC(CreationQuery, DataAttribute->string);
            SdbStringAppendC(CreationQuery, " ");
            SdbStringAppendC(CreationQuery, DataAttribute->valuestring);
            SdbStringAppendC(CreationQuery, ",\n");
        }

        SdbStringBackspace(CreationQuery, 2);
        SdbStringAppendC(CreationQuery, ");");
        SdbPrintfDebug("Table creation query:\n%s\n", CreationQuery);

        PGresult *PgRes = PQexec(PgCtx->DbConn, CreationQuery);
        if(PQresultStatus(PgRes) != PGRES_COMMAND_OK) {
            SdbLogError("Table creation failed: %s", PQerrorMessage(PgCtx->DbConn));
            SdbScratchRelease(Scratch);
            PQclear(PgRes);
            return -1;
        } else {
            SdbLogInfo("Table '%s' created successfully (or it already existed).",
                       SensorName->valuestring);
            PQclear(PgRes);
        }


        Ti->ColMetadata
            = GetTableMetadata(PgCtx->DbConn, Ti->TableName, (int *)&Ti->ColCount, &Pg->Arena);
        if(NULL == Ti->ColMetadata || 0 == Ti->ColCount) {
            SdbLogError("Failed to get column metadata for table %s", Ti->TableName);
            SdbScratchRelease(Scratch);
            return -1;
        }

        sensor_data_pipe *Pipe = Pg->SdPipes[SensorIdx];

        Pipe->PacketSize = Ti->ColMetadata[Ti->ColCount - 1].Offset
                         + Ti->ColMetadata[Ti->ColCount - 1].TypeLength;
        Pipe->PacketMaxCount = Pipe->Buffers[0]->Cap / Pipe->PacketSize;
        Pipe->BufferMaxFill  = Pipe->PacketSize * Pipe->PacketMaxCount;


        sdb_string PrepareStatement = SdbStringMake(Scratch.Arena, NULL);
        SdbStringAppendFmt(PrepareStatement, "COPY %s FROM STDIN WITH (FORMAT binary)",
                           Ti->TableName);

        PgRes = PQprepare(PgCtx->DbConn, Ti->Statement, PrepareStatement, 0, NULL);
        if(PQresultStatus(PgRes) != PGRES_COMMAND_OK) {
            SdbLogError("Failed to prepare COPY statement: %s", PQerrorMessage(PgCtx->DbConn));
            PQclear(PgRes);
            SdbScratchRelease(Scratch);
            return -1;
        }

        PQclear(PgRes);
        ++SensorIdx;
    }

    cJSON_Delete(SchemaConf);
    SdbScratchRelease(Scratch);

    return 0;

JSON_err:
    SdbLogError("Sensor schema JSON incorrect, cJSON error: %s", cJSON_GetErrorPtr());
    cJSON_Delete(SchemaConf);
    PQfinish(PgCtx->DbConn);
    SdbScratchRelease(Scratch);
    return -1;
}

sdb_errno
PgInsertData(PGconn *Conn, pg_table_info *Ti, sensor_data_pipe *Pipe)
{
    PGresult *PgRes;
    sdb_errno Ret = 0;

    PgRes = PQexec(Conn, "BEGIN");
    if(PQresultStatus(PgRes) != PGRES_COMMAND_OK) {
        SdbLogError("Failed to begin transaction for insertion into table %s. Pg error: %s",
                    Ti->TableName, PQerrorMessage(Conn));
        PQclear(PgRes);
        return -SDBE_PG_ERROR;
    }
    PQclear(PgRes);

    PgRes = PQexecPrepared(Conn, Ti->Statement, 0, NULL, NULL, NULL, 0);
    if(PQresultStatus(PgRes) != PGRES_COMMAND_OK) {
        SdbLogError("Failed to execute prepared copy statement for table %s. Pg error: %s",
                    Ti->TableName, PQerrorMessage(Conn));
        PQclear(PgRes);
        PQexec(Conn, "ROLLBACK");
        return -SDBE_PG_ERROR;
    }
    PQclear(PgRes);


    char     CopyHeader[19]       = "PGCOPY\n\377\r\n\0";
    uint32_t CopyFlags            = 0;
    uint32_t CopyExtenstionLength = 0;

    SdbMemcpy(CopyHeader + 11, &CopyFlags, sizeof(CopyFlags));
    SdbMemcpy(CopyHeader + 15, &CopyExtenstionLength, sizeof(CopyExtenstionLength));

    if(PQputCopyData(Conn, CopyHeader, 19) != 1) {
        SdbLogError("Failed to send COPY binary header for table %s. Pg error: %s", Ti->TableName,
                    PQerrorMessage(Conn));
        Ret = -SDBE_PG_ERROR;
        goto cleanup;
    }


    sdb_arena *Buf = NULL;
    while((Buf = SdPipeGetReadBuffer(Pipe)) != NULL) {
        if(PQputCopyData(Conn, (const char *)Buf->Mem, Buf->Cur) != 1) {
            Ret = -1;
            goto cleanup;
        }
    }


cleanup:
    if(PQputCopyEnd(Conn, (Ret == 0) ? NULL : "Error during copy") != 1) {
        SdbLogError("Failed to end COPY for table %s. Pg error: %s", Ti->TableName,
                    PQerrorMessage(Conn));
        Ret = -SDBE_PG_ERROR;
    }

    PgRes = PQgetResult(Conn);
    if(PQresultStatus(PgRes) != PGRES_COMMAND_OK) {
        SdbLogError("COPY command failed for table %s. Pg error: %s", Ti->TableName,
                    PQerrorMessage(Conn));
        Ret = -SDBE_PG_ERROR;
    }
    PQclear(PgRes);

    if(Ret == 0) {
        PgRes = PQexec(Conn, "COMMIT");
    } else {
        PgRes = PQexec(Conn, "ROLLBACK");
    }

    if(PQresultStatus(PgRes) != PGRES_COMMAND_OK) {
        SdbLogError("Failed to end transaction for table %s. Pg error: %s", Ti->TableName,
                    PQerrorMessage(Conn));
        Ret = -SDBE_PG_ERROR;
    }
    PQclear(PgRes);

    if(Ret == 0) {
        SdbLogInfo("Committed copy transaction for table %s", Ti->TableName);
    } else {
        SdbLogWarning("Rolled back copy transaction for table %s", Ti->TableName);
    }

    return Ret;
}

sdb_errno
PgMainLoop(database_api *Pg)
{
    // TODO(ingar): Scratch arenas ?
    sdb_errno Ret = 0;
#if 0
    postgres_ctx *PgCtx = PG_CTX(Pg);

    int ReadEventFd = Pipe->ReadEventFd;
    SdbLogDebug("Starting sensor thread for table %s", PtCtx->TableInfo.TableName);

    int EpollFd = epoll_create1(0);
    if(EpollFd == -1) {
        SdbLogError("Failed to create epoll fd for sensor %s: %s", PtCtx->TableInfo.TableName,
                    strerror(errno));
        return -1;
    }

    struct epoll_event ReadEvent = { .events = EPOLLIN, .data.fd = ReadEventFd };
    if(epoll_ctl(EpollFd, EPOLL_CTL_ADD, ReadEventFd, &ReadEvent) == -1) {
        SdbLogError("Failed to create epoll event for sensor %s: %s", PtCtx->TableInfo.TableName,
                    strerror(errno));
        return -1;
    }

    struct epoll_event Events[1];
    while(!SdbTCtlShouldStop(PtCtx->Control)) {
        static int LogCounter = 0;
        if(++LogCounter % 1000 == 0) {
            SdbLogDebug("Sensor thread for %s still running", PtCtx->TableInfo.TableName);
        }

        // TODO(ingar): Is 1000 appropriate timeout?
        int EpollRet = epoll_wait(EpollFd, Events, 1, 1000);
        if(EpollRet == -1) {
            if(errno == EINTR) {
                // Check stop condition again after interrupt
                if(SdbTCtlShouldStop(PtCtx->Control)) {
                    SdbLogDebug("Stop signal received for %s", PtCtx->TableInfo.TableName);
                    break;
                }
                continue;
            } else {
                SdbLogError("Epoll wait returned with error for sensor %s: %s",
                            PtCtx->TableInfo.TableName, strerror(errno));
                Ret = -errno;
                break;
            }
        } else if(EpollRet == 0) {
            // Check stop condition on timeout
            if(SdbTCtlShouldStop(PtCtx->Control)) {
                SdbLogDebug("Stop signal received for %s", PtCtx->TableInfo.TableName);
                break;
            }
            continue;
        }

        Ret = PgInsertData(PtCtx);
        if(Ret != 0) {
            // TODO(ingar): Try again?
        }
    }

    SdbLogDebug("Sensor thread for %s stopping", PtCtx->TableInfo.TableName);
    close(EpollFd);
    SdbTCtlMarkStopped(PtCtx->Control);
#endif
    return Ret;
}
