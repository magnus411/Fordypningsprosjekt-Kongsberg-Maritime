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
#define SDB_LOG_LEVEL 4
#include <SdbExtern.h>
#include <modules/DatabaseModule.h>
#include <database_systems/Postgres.h>

SDB_LOG_REGISTER(Postgres);

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
    if(Result == NULL) {
        SdbLogError("PGresult is NULL\n");
        return;
    }

    // NOTE(ingar): Since it's a printing function, using printf instead of logging functions is
    ExecStatusType Status = PQresultStatus(Result);
    SdbPrintfDebug("Result Status: %s\n", PQresStatus(Status));

    if(Status == PGRES_FATAL_ERROR || Status == PGRES_NONFATAL_ERROR) {
        SdbLogError("Error Message: %s\n", PQresultErrorMessage(Result));
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
PqTableMetaDataQuery(const char *TableName, u64 TableNameLen)
{
    u64   QueryLen = PqTableMetadataQueryFmtLen__ + TableNameLen;
    char *QueryBuf = (char *)malloc(QueryLen);
    snprintf(QueryBuf, QueryLen, PqTableMetadataQueryFmt__, TableName);

    return QueryBuf;
}

void
PrintColumnMetadata(const pq_col_metadata *Metadata)
{
    SdbPrintfDebug("Table OID: %u\n", Metadata->TableOid);
    SdbPrintfDebug("Table Name: %s\n", Metadata->TableName);
    SdbPrintfDebug("Column Number: %d\n", Metadata->ColumnNumber);
    SdbPrintfDebug("Column Name: %s\n", Metadata->ColumnName);
    SdbPrintfDebug("Type OID: %u\n", Metadata->TypeOid);
    SdbPrintfDebug("Type Name: %s\n", Metadata->TypeName);
    SdbPrintfDebug("Type Length: %d\n", Metadata->TypeLength);
    SdbPrintfDebug("Type By Value: %s\n", Metadata->TypeByValue ? "Yes" : "No");
    SdbPrintfDebug("Type Alignment: %c\n", Metadata->TypeAlignment);
    SdbPrintfDebug("Type Storage: %c\n", Metadata->TypeStorage);
    SdbPrintfDebug("Type Modifier: %d\n", Metadata->TypeModifier);
    SdbPrintfDebug("Not Null: %s\n", Metadata->NotNull ? "Yes" : "No");
    SdbPrintfDebug("Full Data Type: %s\n", Metadata->FullDataType);
    SdbPrintfDebug("\n");
}

pq_col_metadata *
GetTableMetadata(PGconn *DbConn, const char *TableName, u64 TableNameLen, int *ColCount)
{
    /* Typical libpq convention that variables for sizes are passed in an filled out by the
     * function. Not used here atm */

    const char *MetadataQuery = PqTableMetaDataQuery(TableName, TableNameLen);
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
    pq_col_metadata *MetadataArray = malloc(RowCount * sizeof(pq_col_metadata));
    if(!MetadataArray) {
        SdbLogError("Memory allocation failed");
        PQclear(Result);
        return NULL;
    }

    SdbLogDebug("Array allocated for %d columns!", RowCount);

    for(int RowIndex = 0; RowIndex < RowCount; RowIndex++) {
        pq_col_metadata *CurrentMetadata = &MetadataArray[RowIndex];

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

void
InsertSensorData(PGconn *DbConn, const char *TableName, u64 TableNameLen, const u8 *SensorData,
                 size_t DataSize)
{
    int              NTableCols;
    pq_col_metadata *ColMetadata = GetTableMetadata(DbConn, TableName, TableNameLen, &NTableCols);
    if(NULL == ColMetadata) {
        SdbLogError("Failed to get column metadata for table %s", TableName);
        return;
    }
    if(0 == NTableCols) {
        SdbLogError("Table had no columns!");
        return;
    }

    char Query[1024] = "INSERT INTO "; // TODO(ingar): Better memory management
    strcat(Query, TableName);
    strcat(Query, " (");
    for(int i = 0; i < NTableCols; i++) {
        if(i > 0) {
            strcat(Query, ", ");
        }
        strcat(Query, ColMetadata[i].ColumnName);
    }

    strcat(Query, ") VALUES (");
    for(int i = 0; i < NTableCols; i++) {
        if(i > 0) {
            strcat(Query, ", ");
        }
        strcat(Query, "$");
        char ParamNumber[8];
        snprintf(ParamNumber, sizeof(ParamNumber), "%lu", (u64)(i + 1));
        strcat(Query, ParamNumber);
    }
    strcat(Query, ")");

    SdbLogDebug("%s", Query);

    PGresult *PqPrepareResult = PQprepare(DbConn, "", Query, NTableCols, NULL);
    if(PQresultStatus(PqPrepareResult) != PGRES_COMMAND_OK) {
        SdbLogError("Prepare failed: %s", PQerrorMessage(DbConn));
        PQclear(PqPrepareResult);
        return;
    }
    PQclear(PqPrepareResult);

    char **ParamValues  = malloc(NTableCols * sizeof(char *));
    int   *ParamLengths = malloc(NTableCols * sizeof(int));
    int   *ParamFormats = malloc(NTableCols * sizeof(int));
    size_t ParamOffset  = 0;

    for(int i = 0; i < NTableCols; i++) {
        if((ParamOffset + ColMetadata[i].TypeLength) > DataSize) {
            SdbLogError("Buffer overflow at column %s\n", ColMetadata[i].ColumnName);
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
            SdbLogDebug("Attempting to inser %lu chars into varchar", ParamLengths[i]);
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
        SdbLogDebug("Data inserted successfully\n");
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

char *
GetSqlQueryFromFile(const char *FileName, sdb_arena *Arena)
{
    sdb_file_data *File = SdbLoadFileIntoMemory(FileName, Arena);
    return (char *)File->Data;
}

sdb_errno
PgInsert(database_api *PgApi)
{
    pg_db_api_ctx *PgContext = PgApi->Context;
    pthread_mutex_lock(&PgContext->CtxLock);

    ssize_t CbReadRet = CbRead(PgContext->Cb, PgContext->PgInsertBuf, PgContext->PgInsertBufSize);
    if(CbReadRet < 0) {
        SdbLogError("Failed to read from circular buffer");
    }

    pthread_mutex_unlock(&PgContext->CtxLock);

    return 0;
}

sdb_errno
PgInit(database_api *PgApi)
{
    // TODO(ingar): Make

    return 0;
}
