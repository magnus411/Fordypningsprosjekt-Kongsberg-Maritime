#include <arpa/inet.h>
#include <libpq-fe.h>
#include <stdlib.h>
#include <string.h>

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
#include <src/DatabaseSystems/DatabaseInitializer.h>
#include <src/DatabaseSystems/Postgres.h>
#include <src/Libs/cJSON/cJSON.h>
#include <src/Modules/DatabaseModule.h>

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

char *
PqTableMetaDataFull(const char *TableName, u64 TableNameLen)
{
    u64   QueryLen = PG_TABLE_METADATA_FULL_QUERY_FMT_LEN + TableNameLen;
    char *QueryBuf = (char *)malloc(QueryLen);
    snprintf(QueryBuf, QueryLen, PG_TABLE_METADATA_FULL_QUERY_FMT, TableName);

    return QueryBuf;
}

void
PrintColumnMetadataFull(const pg_col_metadata_full *Metadata)
{
    printf("\n");
    printf("Table OID: %u\n", Metadata->TableOid);
    printf("Table Name: %s\n", Metadata->TableName);
    printf("Column Number: %d\n", Metadata->ColumnNumber);
    printf("Column Name: %s\n", Metadata->ColumnName);
    printf("Type OID: %u\n", Metadata->TypeOid);
    printf("Type Name: %s\n", Metadata->TypeName);
    printf("Type Length: %d\n", Metadata->TypeLength);
    printf("Type By Value: %s\n", Metadata->TypeByValue ? "Yes" : "No");
    printf("Type Alignment: %c\n", Metadata->TypeAlignment);
    printf("Type Storage: %c\n", Metadata->TypeStorage);
    printf("Type Modifier: %d\n", Metadata->TypeModifier);
    printf("Not Null: %s\n", Metadata->NotNull ? "Yes" : "No");
    printf("Full Data Type: %s\n", Metadata->FullDataType);
    printf("\n");
}

pg_col_metadata_full *
GetTableMetadataFull(PGconn *DbConn, const char *TableName, u64 TableNameLen, int *ColCount)
{
    /* Typical libpq convention that variables for sizes are passed in an filled out by the
     * function. Not used here atm */

    const char *MetadataQuery = PqTableMetaDataFull(TableName, TableNameLen);
    PGresult   *Result        = PQexec(DbConn, MetadataQuery);
    if(PQresultStatus(Result) != PGRES_TUPLES_OK) {
        SdbLogError("Query failed: %s", PQerrorMessage(DbConn));
        PQclear(Result);
    }

    SdbLogDebug("Metadata query successful!");

    PrintPGresult(Result);

    int RowCount = PQntuples(Result);
    *ColCount
        = RowCount; // NOTE(ingar): Each row is the metadata for one column in the requested table
    pg_col_metadata_full *MetadataArray = malloc(RowCount * sizeof(pg_col_metadata_full));
    if(!MetadataArray) {
        SdbLogError("Memory allocation failed");
        PQclear(Result);
        return NULL;
    }

    SdbLogDebug("Array allocated for %d columns!", RowCount);

    for(int RowIndex = 0; RowIndex < RowCount; RowIndex++) {
        pg_col_metadata_full *CurrentMetadata = &MetadataArray[RowIndex];

        CurrentMetadata->TableOid      = (u32)atoi(PQgetvalue(Result, RowIndex, 0));
        CurrentMetadata->TableName     = strdup(PQgetvalue(Result, RowIndex, 1));
        CurrentMetadata->ColumnNumber  = (i16)atoi(PQgetvalue(Result, RowIndex, 2));
        CurrentMetadata->ColumnName    = strdup(PQgetvalue(Result, RowIndex, 3));
        CurrentMetadata->TypeOid       = (u32)atoi(PQgetvalue(Result, RowIndex, 4));
        CurrentMetadata->TypeName      = strdup(PQgetvalue(Result, RowIndex, 5));
        CurrentMetadata->TypeLength    = (i16)atoi(PQgetvalue(Result, RowIndex, 6));
        CurrentMetadata->TypeByValue   = strcmp(PQgetvalue(Result, RowIndex, 7), "t") == 0;
        CurrentMetadata->TypeAlignment = PQgetvalue(Result, RowIndex, 8)[0];
        CurrentMetadata->TypeStorage   = PQgetvalue(Result, RowIndex, 9)[0];
        CurrentMetadata->TypeModifier  = (i32)atoi(PQgetvalue(Result, RowIndex, 10));
        CurrentMetadata->NotNull       = strcmp(PQgetvalue(Result, RowIndex, 11), "t") == 0;
        CurrentMetadata->FullDataType  = strdup(PQgetvalue(Result, RowIndex, 12));
    }

    PQclear(Result);

    return MetadataArray;
}

#define PQ_TABLE_METADATA_QUERY_FMT                                                                \
    "SELECT a.attname AS column_name, t.oid AS type_oid, t.typlen AS type_length, "                \
    "a.atttypmod AS type_modifier FROM pg_catalog.pg_class c "                                     \
    "JOIN pg_catalog.pg_attribute a ON a.attrelid = c.oid "                                        \
    "JOIN pg_catalog.pg_type t ON a.atttypid = t.oid "                                             \
    "WHERE c.relname = '%s' AND a.attnum > 0 AND NOT a.attisdropped "                              \
    "AND a.attname != 'id' ORDER BY a.attnum"

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
    // NOTE(ingar): Each row is the metadata for one column in the requested table
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
    }

    PQclear(Result);

    return MetadataArray;
}

sdb_errno
PrepareStatements(database_api *Pg)
{
    postgres_ctx *PgCtx       = PG_CTX(Pg);
    PGconn       *DbConn      = PgCtx->DbConn;
    sdb_string   *TableNames  = PgCtx->TableNames;
    u64           TableCount  = PgCtx->TableCount;
    PgCtx->PreparedStatements = SdbPushArray(&Pg->Arena, sdb_string, PgCtx->TableCount);
    sdb_scratch_arena Scratch = SdbScratchGet(NULL, 0);

    for(u64 TableIdx = 0; TableIdx < TableCount; ++TableIdx) {
        sdb_string TableName = TableNames[TableIdx];

        int              NTableCols = 0;
        pg_col_metadata *ColMetadata
            = GetTableMetadata(DbConn, TableName, &NTableCols, Scratch.Arena);
        if(NULL == ColMetadata || 0 == NTableCols) {
            SdbLogError("Failed to get column metadata for table %s", TableName);
            return -1;
        }

        sdb_string StatementString = SdbStringMake(Scratch.Arena, "INSERT INTO ");

        SdbStringAppend(StatementString, TableName);
        SdbStringAppendC(StatementString, " (");
        for(int i = 0; i < NTableCols; i++) {
            if(i > 0) {
                SdbStringAppendC(StatementString, ", ");
            }
            SdbStringAppendC(StatementString, ColMetadata[i].ColumnName);
        }

        SdbStringAppendC(StatementString, ") VALUES (");
        for(int i = 0; i < NTableCols; i++) {
            if(i > 0) {
                SdbStringAppendC(StatementString, ", ");
            }
            SdbStringAppendFmt(StatementString, "$%lu", (u64)(i + 1));
        }
        SdbStringAppendC(StatementString, ")");

        SdbLogDebug("%s", StatementString);
        PgCtx->PreparedStatements[TableIdx] = SdbStringMake(&Pg->Arena, TableName);
        SdbStringAppendC(PgCtx->PreparedStatements[TableIdx], "_insert");

        PGresult *PqPrepareResult = PQprepare(DbConn, PgCtx->PreparedStatements[TableIdx],
                                              StatementString, NTableCols, NULL);
        if(PQresultStatus(PqPrepareResult) != PGRES_COMMAND_OK) {
            SdbLogError("Prepare failed: %s", PQerrorMessage(DbConn));
            PQclear(PqPrepareResult);
            SdbScratchRelease(Scratch);
            return -1;
        }

        PgCtx->StatementCount += 1;
        PQclear(PqPrepareResult);
    }

    SdbScratchRelease(Scratch);
    return 0;
}
#if 0
void
InsertSensorData(PGconn *DbConn, const char *TableName, u64 TableNameLen, const u8 *SensorData,
                 size_t DataSize)
{
    int              NTableCols;
    char **ParamValues  = malloc(NTableCols * sizeof(char *));
    int   *ParamLengths = malloc(NTableCols * sizeof(int));
    int   *ParamFormats = malloc(NTableCols * sizeof(int));
    size_t ParamOffset  = 0;

    for(int i = 0; i < NTableCols; i++) {
        if((ParamOffset + ColMetadata[i].TypeLength) > DataSize) {
            SdbLogError("Buffer overflow at column %s", ColMetadata[i].ColumnName);
            break;
        }

        ParamValues[i]  = (char *)SensorData + ParamOffset;
        ParamLengths[i] = ColMetadata[i].TypeLength;
        // Use binary format as default. Will changed to 0 if a column is varchar
        ParamFormats[i] = 1;

        // NOTE(ingar): Converts to network order. This modifies the buffer directly, which
        // is what we want, since there is not extra memory allocation
        if((ColMetadata[i].TypeOid == INT4) || (ColMetadata[i].TypeOid == INT8)
           || (ColMetadata[i].TypeOid == FLOAT8)) {
            if(4 == ColMetadata[i].TypeLength) {
                *(uint32_t *)(SensorData + ParamOffset)
                    = htonl(*(uint32_t *)(SensorData + ParamOffset));
            } else if(8 == ColMetadata[i].TypeLength) {
                *(uint64_t *)(SensorData + ParamOffset)
                    = htobe64(*(uint64_t *)(SensorData + ParamOffset));
            }
        }

        if(ColMetadata[i].TypeOid == VARCHAR) {
            const char *StringStart = ParamValues[i];
            u64         VarCharLen  = ColMetadata[i].TypeModifier - 4;
            ParamLengths[i]         = SdbStrnlen(StringStart, VarCharLen);
            ParamFormats[i]         = 0; // NOTE(ingar): Sets to use text format instead of binary
            SdbLogDebug("Attempting to inser %d chars into varchar", ParamLengths[i]);
        }

        ParamOffset += ColMetadata[i].TypeLength;
        SdbLogDebug("Insert %d: ParamValue %p, ParamLength %d, ParamType %d, ParamFormat %d", i,
                    ParamValues[i], ParamLengths[i], ColMetadata[i].TypeOid, ParamFormats[i]);
    }

    PqPrepareResult = PQexecPrepared(DbConn, "", NTableCols, (const char *const *)ParamValues,
                                     ParamLengths, ParamFormats, 1);

    if(PQresultStatus(PqPrepareResult) != PGRES_COMMAND_OK) {
        SdbLogError("Insert failed: %s", PQerrorMessage(DbConn));
    } else {
        SdbLogDebug("Data inserted successfully");
    }

    // Clean up
    PQclear(PqPrepareResult);

    for(int i = 0; i < NTableCols; ++i) {
        pq_col_metadata *CurrentMetadata = &ColMetadata[i];

        free(CurrentMetadata->TableName);
        free(CurrentMetadata->ColumnName);
        free(CurrentMetadata->TypeName);
    }

    free(ColMetadata);
    free(ParamValues);
    free(ParamLengths);
    free(ParamFormats);
}
#endif

sdb_errno
ProcessTablesInConfig(database_api *Pg, cJSON *SchemaConf)
{
    if(!cJSON_IsObject(SchemaConf)) {
        goto JSON_err;
    }

    cJSON *SensorSchemaArray = cJSON_GetObjectItem(SchemaConf, "sensors");
    if(SensorSchemaArray == NULL || !cJSON_IsArray(SensorSchemaArray)) {
        goto JSON_err;
    }

    postgres_ctx *PgCtx = PG_CTX(Pg);
    PgCtx->TableCount   = cJSON_GetArraySize(SensorSchemaArray);
    PgCtx->TableNames   = SdbPushArray(&Pg->Arena, sdb_string, PgCtx->TableCount);

    u64    TableCounter = 0;
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

        sdb_string TableName              = SdbStringMake(&Pg->Arena, SensorName->valuestring);
        PgCtx->TableNames[TableCounter++] = TableName;

        sdb_scratch_arena Scratch = SdbScratchGet(NULL, 0);
        sdb_string CreationQuery  = SdbStringMake(Scratch.Arena, "CREATE TABLE IF NOT EXISTS ");
        (void)SdbStringAppendC(CreationQuery, SensorName->valuestring);
        (void)SdbStringAppendC(CreationQuery, "(\nid SERIAL PRIMARY KEY,\n");

        cJSON *SensorData = cJSON_GetObjectItem(SensorSchema, "data");
        if(SensorData == NULL || !cJSON_IsObject(SensorData)) {
            goto JSON_err;
        }

        cJSON *DataAttribute = NULL;
        cJSON_ArrayForEach(DataAttribute, SensorData)
        {
            if(!cJSON_IsString(DataAttribute)) {
                goto JSON_err;
            }

            (void)SdbStringAppendC(CreationQuery, DataAttribute->string);
            (void)SdbStringAppendC(CreationQuery, " ");
            (void)SdbStringAppendC(CreationQuery, DataAttribute->valuestring);
            (void)SdbStringAppendC(CreationQuery, ",\n");
        }

        SdbStringBackspace(CreationQuery, 2);
        SdbStringAppendC(CreationQuery, ");");
        SdbPrintfDebug("Table creation query:\n%s\n", CreationQuery);

        PGresult *CreateRes = PQexec(PgCtx->DbConn, CreationQuery);
        if(PQresultStatus(CreateRes) != PGRES_COMMAND_OK) {
            SdbLogError("Table creation failed: %s", PQerrorMessage(PgCtx->DbConn));
            PQclear(CreateRes);
            return -1;
        } else {
            SdbLogInfo("Table '%s' created successfully (or it already existed).", TableName);
            PQclear(CreateRes);
        }

        SdbScratchRelease(Scratch);
    }


    return 0;

JSON_err:
    SdbLogError("Sensor schema JSON incorrect, cJSON error: %s", cJSON_GetErrorPtr());
    return -1;
}
