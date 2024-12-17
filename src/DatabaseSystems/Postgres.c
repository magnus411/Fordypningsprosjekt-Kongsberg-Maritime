/**
 * @file Postgres.c
 * @brief Implementation of PostgreSQL database operations
 */

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

#include <src/DatabaseSystems/Postgres.h>
SDB_THREAD_ARENAS_REGISTER(Postgres, 2);

#include <src/Common/SensorDataPipe.h>
#include <src/Common/Thread.h>
#include <src/DatabaseSystems/DatabaseInitializer.h>
#include <src/Libs/cJSON/cJSON.h>

// TODO(ingar): Remove before release
#include <src/DevUtils/TestConstants.h>

void
PgInitThreadArenas(void)
{
    SdbThreadArenasInit(Postgres);
}

/**
 * @brief Converts Unix timestamp to PostgreSQL timestamp
 *
 * Implementation detail: Converts by adjusting for epoch difference and
 * scaling to microseconds
 */
inline pg_timestamp
UnixToPgTimestamp(time_t UnixTime)
{
    pg_timestamp Timestamp = ((UnixTime * USECS_PER_SECOND)
                              + ((UNIX_EPOCH_JDATE - POSTGRES_EPOCH_JDATE) * USECS_PER_DAY));
    return Timestamp;
}

pg_timestamp
TimevalToPgTimestamp(struct timeval Tv)
{
    return ((Tv.tv_sec * USECS_PER_SECOND) + Tv.tv_usec
            + ((UNIX_EPOCH_JDATE - POSTGRES_EPOCH_JDATE) * USECS_PER_DAY));
}

pg_timestamp
TimespecToPgTimestamp(struct timespec Ts)
{
    return ((Ts.tv_sec * USECS_PER_SECOND) + (Ts.tv_nsec / NSECS_PER_USEC)
            + ((UNIX_EPOCH_JDATE - POSTGRES_EPOCH_JDATE) * USECS_PER_DAY));
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
GetTableMetadata(PGconn *DbConn, sdb_string TableName, i16 *ColCount, i16 *ColCountNoAutoIncrements,
                 size_t *RowSize, sdb_arena *A)
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


    *ColCountNoAutoIncrements = 0;
    i32 Offset                = 0;
    for(u64 RowIndex = 0; RowIndex < RowCount; ++RowIndex) {
        pg_col_metadata *CurrentMetadata = &MetadataArray[RowIndex];

        CurrentMetadata->ColumnName      = SdbStringMake(A, PQgetvalue(Result, RowIndex, 0));
        CurrentMetadata->TypeOid         = (pg_oid)atoi(PQgetvalue(Result, RowIndex, 1));
        CurrentMetadata->TypeLength      = (i32)atoi(PQgetvalue(Result, RowIndex, 2));
        CurrentMetadata->TypeModifier    = (i32)atoi(PQgetvalue(Result, RowIndex, 3));
        CurrentMetadata->IsPrimaryKey    = (PQgetvalue(Result, RowIndex, 4)[0] == 't');
        CurrentMetadata->IsAutoIncrement = (PQgetvalue(Result, RowIndex, 5)[0] == 't');

        if(CurrentMetadata->IsAutoIncrement) {
            // NOTE(ingar): The assumption is that auto-incrementing values will not appear in the
            // incoming data
            CurrentMetadata->Offset = -1;
            continue;
        }

        CurrentMetadata->Offset = Offset;
        Offset += CurrentMetadata->TypeLength;
        *RowSize += CurrentMetadata->TypeLength;
        *ColCountNoAutoIncrements += 1;
    }

    PQclear(Result);

    return MetadataArray;
}


postgres_ctx *
PgPrepareCtx(sdb_arena *PgArena, sensor_data_pipe *Pipe)
{
    sdb_errno         Errno   = 0;
    sdb_scratch_arena Scratch = SdbScratchGet(NULL, 0);

    // TODO(ingar): Make pg config a json file??
    sdb_file_data *ConfFile = SdbLoadFileIntoMemory(POSTGRES_CONF_FS_PATH, Scratch.Arena);
    if(ConfFile == NULL) {
        SdbLogError("Failed to open config file");
        SdbScratchRelease(Scratch);
        return NULL;
    }

    cJSON *SchemaConf = DbInitGetConfFromFile("./configs/sensor_schemas.json", Scratch.Arena);
    if(!cJSON_IsObject(SchemaConf)) {
        Errno = -SDBE_JSON_ERR;
        goto cleanup;
    }

    cJSON *SensorSchemaArray = cJSON_GetObjectItem(SchemaConf, "sensors");
    if(SensorSchemaArray == NULL || !cJSON_IsArray(SensorSchemaArray)) {
        Errno = -SDBE_JSON_ERR;
        goto cleanup;
    }

    u64           SensorCount = cJSON_GetArraySize(SensorSchemaArray);
    postgres_ctx *PgCtx       = SdbPushStruct(PgArena, postgres_ctx);
    PgCtx->TablesInfo         = SdbPushArray(PgArena, pg_table_info *, SensorCount);

    const char *ConnInfo = (const char *)ConfFile->Data;
    PgCtx->DbConn        = PQconnectdb(ConnInfo);

    if(PQstatus(PgCtx->DbConn) != CONNECTION_OK) {
        SdbLogError("PgCtx->DbConnection to database failed. PQ error:\n%s",
                    PQerrorMessage(PgCtx->DbConn));
        Errno = -SDBE_PG_ERR;
        goto cleanup;
    }

    u64    SensorIdx    = 0;
    cJSON *SensorSchema = NULL;
    cJSON_ArrayForEach(SensorSchema, SensorSchemaArray)
    {
        if(!cJSON_IsObject(SensorSchema)) {
            Errno = -SDBE_JSON_ERR;
            goto cleanup;
        }

        cJSON *SensorName = cJSON_GetObjectItem(SensorSchema, "name");
        if(SensorName == NULL || !cJSON_IsString(SensorName)) {
            Errno = -SDBE_JSON_ERR;
            goto cleanup;
        }


        pg_table_info *Ti            = SdbPushStruct(PgArena, pg_table_info);
        Ti->TableName                = SdbStringMake(PgArena, SensorName->valuestring);
        PgCtx->TablesInfo[SensorIdx] = Ti;

        sdb_string CreationQuery = SdbStringMake(Scratch.Arena, "CREATE TABLE IF NOT EXISTS ");
        SdbStringAppendC(CreationQuery, SensorName->valuestring);
        SdbStringAppendC(CreationQuery, "(\nid SERIAL PRIMARY KEY,\n");

        cJSON *SensorData = cJSON_GetObjectItem(SensorSchema, "data");
        if(SensorData == NULL || !cJSON_IsObject(SensorData)) {
            Errno = -SDBE_JSON_ERR;
            goto cleanup;
        }

        cJSON *DataAttribute = NULL;
        cJSON_ArrayForEach(DataAttribute, SensorData)
        {
            if(!cJSON_IsString(DataAttribute)) {
                Errno = -SDBE_JSON_ERR;
                goto cleanup;
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
            Errno = -SDBE_PG_ERR;
            PQclear(PgRes);
            goto cleanup;
        } else {
            SdbLogInfo("Table '%s' created successfully (or it already existed).",
                       SensorName->valuestring);
            PQclear(PgRes);
        }


        Ti->ColMetadata = GetTableMetadata(PgCtx->DbConn, Ti->TableName, &Ti->ColCount,
                                           &Ti->ColCountNoAutoIncrements, &Ti->RowSize, PgArena);

        if(NULL == Ti->ColMetadata || 0 == Ti->ColCount) {
            SdbLogError("Failed to get column metadata for table %s", Ti->TableName);
            Errno = -SDBE_PG_ERR;
            goto cleanup;
        }

        Ti->CopyCommand = SdbStringMake(PgArena, "COPY ");
        SdbStringAppend(Ti->CopyCommand, Ti->TableName);
        SdbStringAppendC(Ti->CopyCommand, "(");
        for(u64 c = 0; c < Ti->ColCount; ++c) {
            pg_col_metadata ColMd = Ti->ColMetadata[c];
            if(!ColMd.IsAutoIncrement) {
                SdbStringAppend(Ti->CopyCommand, ColMd.ColumnName);
                SdbStringAppendC(Ti->CopyCommand, ", ");
            }
        }
        SdbStringBackspace(Ti->CopyCommand, 2);
        SdbStringAppendC(Ti->CopyCommand, ") FROM STDIN WITH (FORMAT binary)");

        // NOTE(ingar): An assumption made is that each sensor will have its own pipe since we don't
        // have a method of differentiating packets at the moment, but unfortunately we probably
        // don't have time to complete this part. This means that the max number of sensors
        // supported in this current implementation is 1, BUT extending it to support more should be
        // relatively simple.
        Pipe->PacketSize    = Ti->RowSize;
        Pipe->ItemMaxCount  = Pipe->Buffers[0]->Cap / Pipe->PacketSize;
        Pipe->BufferMaxFill = Pipe->PacketSize * Pipe->ItemMaxCount;

        ++SensorIdx;
    }

cleanup:
    if(Errno == -SDBE_JSON_ERR) {
        SdbLogError("Encountered JSON error from sensor schema, cJSON error: %s",
                    cJSON_GetErrorPtr());
    }
    if(SchemaConf != NULL) {
        cJSON_Delete(SchemaConf);
    }
    if(Errno != 0) {
        PQfinish(PgCtx->DbConn);
    }

    SdbScratchRelease(Scratch);
    return (Errno == 0) ? PgCtx : NULL;
}


/**
 * @brief Writes a 2-byte value in network byte order
 *
 * @param To Destination buffer
 * @param From Source buffer
 * @return Number of bytes written
 */
static inline size_t
Write2(char *To, const char *From)
{
    u16 Val;
    SdbMemcpy(&Val, From, sizeof(Val));
    Val = htobe16(Val);
    SdbMemcpy(To, &Val, sizeof(Val));
    return sizeof(Val);
}

static inline size_t
Write4(char *To, const char *From)
{
    u32 Val;
    SdbMemcpy(&Val, From, sizeof(Val));
    Val = htobe32(Val);
    SdbMemcpy(To, &Val, sizeof(Val));
    return sizeof(Val);
}

static inline size_t
Write8(char *To, const char *From)
{
    u64 Val;
    SdbMemcpy(&Val, From, sizeof(Val));
    Val = htobe64(Val);
    SdbMemcpy(To, &Val, sizeof(Val));
    return sizeof(Val);
}

static inline size_t
WriteTimestamp(char *To, const char *From)
{
    time_t UTime;
    SdbMemcpy(&UTime, From, sizeof(UTime));
    pg_timestamp Val = UnixToPgTimestamp(UTime);
    Val              = htobe64(Val);
    SdbMemcpy(To, &Val, sizeof(Val));
    return sizeof(Val);
}


/**
 * @brief Inserts data into PostgreSQL table using COPY protocol
 *
 * Implements bulk data insertion using PostgreSQL's binary COPY protocol:
 * 1. Begins transaction
 * 2. Initiates COPY operation
 * 3. Converts data to network byte order
 * 4. Sends data in binary format
 * 5. Commits or rolls back transaction
 *
 * @param Conn Database connection
 * @param Ti Table information
 * @param Data Raw data to insert
 * @param ItemCount Number of items to insert
 * @return 0 on success, error code on failure
 */
sdb_errno
PgInsertData(PGconn *Conn, pg_table_info *Ti, const char *Data, u64 ItemCount)
{
    PGresult *PgRes;
    sdb_errno Ret = 0;

    PgRes = PQexec(Conn, "BEGIN");
    if(PQresultStatus(PgRes) != PGRES_COMMAND_OK) {
        SdbLogError("Failed to begin transaction for insertion into table %s. Pg error: %s",
                    Ti->TableName, PQerrorMessage(Conn));
        PQclear(PgRes);
        return -SDBE_PG_ERR;
    }
    PQclear(PgRes);


    PgRes = PQexec(Conn, Ti->CopyCommand);
    if(PQresultStatus(PgRes) != PGRES_COPY_IN) {
        SdbLogError("Failed to start COPY operation for table %s. Pg error: %s", Ti->TableName,
                    PQerrorMessage(Conn));
        PQclear(PgRes);
        PQexec(Conn, "ROLLBACK");
        return -SDBE_PG_ERR;
    }
    PQclear(PgRes);


    char CopyHeader[19]       = "PGCOPY\n\377\r\n\0";
    u32  CopyFlags            = htonl(0);
    u32  CopyExtenstionLength = htonl(0);

    SdbMemcpy(CopyHeader + 11, &CopyFlags, sizeof(CopyFlags));
    SdbMemcpy(CopyHeader + 15, &CopyExtenstionLength, sizeof(CopyExtenstionLength));

    if(PQputCopyData(Conn, CopyHeader, 19) != 1) {
        SdbLogError("Failed to send COPY binary header for table %s. Pg error: %s", Ti->TableName,
                    PQerrorMessage(Conn));
        Ret = -SDBE_PG_ERR;
        goto cleanup;
    }

    size_t PgHeaderSize
        = sizeof(Ti->ColCount) + (ItemCount * sizeof(Ti->ColMetadata[0].TypeLength));
    size_t NtwrkConvBufSize = PgHeaderSize + (ItemCount * Ti->RowSize);

    sdb_scratch_arena NtwrkBufArena = SdbScratchGet(NULL, 0);
    char             *NtwrkConvBuf  = SdbPushArrayZero(NtwrkBufArena.Arena, char, NtwrkConvBufSize);
    if(NtwrkConvBuf == NULL) {
        SdbLogError("Scratch arena has insufficient space for network conversion buffer (need "
                    "%zd bytes). Re-evaluate arena buffer size",
                    NtwrkConvBufSize);
        SdbScratchRelease(NtwrkBufArena);
        goto cleanup;
    }

    size_t ConvOffset = 0;
    for(u64 RowsProcessed = 0; RowsProcessed < ItemCount; ++RowsProcessed) {
        const char *CurrRow = (const char *)Data + (RowsProcessed * Ti->RowSize);

        i16 NtwrkFieldCount = htons(Ti->ColCountNoAutoIncrements);
        SdbMemcpy(NtwrkConvBuf + ConvOffset, &NtwrkFieldCount, sizeof(NtwrkFieldCount));
        ConvOffset += sizeof(NtwrkFieldCount);

        for(u64 c = 0; c < Ti->ColCount; ++c) {
            pg_col_metadata ColMd = Ti->ColMetadata[c];

            if(ColMd.IsAutoIncrement) {
                continue;
            }

            i32 NtwrkTypeLen = htobe32(ColMd.TypeLength);
            SdbMemcpy(NtwrkConvBuf + ConvOffset, &NtwrkTypeLen, sizeof(NtwrkTypeLen));
            ConvOffset += sizeof(NtwrkTypeLen);

            // WARN: The type length might not necessarily be the same as the type written to the
            // network conversion buffer, so we might need to add handling of that
            const char *ColData = CurrRow + ColMd.Offset;
            switch(ColMd.TypeOid) {
                case PG_INT8:
                case PG_FLOAT8:
                    ConvOffset += Write8(NtwrkConvBuf + ConvOffset, ColData);
                    break;
                case PG_INT4:
                case PG_FLOAT4:
                    ConvOffset += Write4(NtwrkConvBuf + ConvOffset, ColData);
                    break;
                case PG_INT2:
                    ConvOffset += Write2(NtwrkConvBuf + ConvOffset, ColData);
                    break;
                case PG_TIMESTAMP:
                    // NOTE(ingar): Support for incoming timestamps of different types
                    // (timeval, timespec, time_t, other) is probably outside the scope of
                    // this project, since I think it will be quite tricky.
                    // I think it's best if we just set it to be time_t, since it's a single
                    // value (unlike timeval and timespec)
                    ConvOffset += WriteTimestamp(NtwrkConvBuf + ConvOffset, ColData);
                    break;
                default:
                    SdbLogWarning("Encountered unhandled or invalid PG oid %d", ColMd.TypeOid);
                    break;
            }
        }
    }

    if(PQputCopyData(Conn, NtwrkConvBuf, ConvOffset) != 1) {
        SdbLogError("Unable to copy converted data for table %s. Pg error: %s", Ti->TableName,
                    PQerrorMessage(Conn));

        Ret = -SDBE_PG_ERR;
        SdbScratchRelease(NtwrkBufArena);
        goto cleanup;
    }

    SdbScratchRelease(NtwrkBufArena);


cleanup:
    if(PQputCopyEnd(Conn, (Ret == 0) ? NULL : "Error during copy") != 1) {
        SdbLogError("Failed to end COPY for table %s. Pg error: %s", Ti->TableName,
                    PQerrorMessage(Conn));
        Ret = -SDBE_PG_ERR;
    }

    PgRes = PQgetResult(Conn);
    if(PQresultStatus(PgRes) != PGRES_COMMAND_OK) {
        SdbLogError("COPY command failed for table %s. Pg error: %s", Ti->TableName,
                    PQerrorMessage(Conn));
        Ret = -SDBE_PG_ERR;
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
        Ret = -SDBE_PG_ERR;
    }
    PQclear(PgRes);

    if(Ret == 0) {
        SdbLogDebug("Committed copy transaction for table %s", Ti->TableName);
    } else {
        SdbLogWarning("Rolled back copy transaction for table %s", Ti->TableName);
    }

    return Ret;
}
