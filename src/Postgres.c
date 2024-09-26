#define SDB_LOG_LEVEL 4
#include "SdbExtern.h"
#include "Postgres.h"

#include <libpq-fe.h>

SDB_LOG_REGISTER(Postgres);

void
DiagnoseConnectionAndTable(PGconn *DbConn, const char *TableName)
{
    printf("Connected to database: %s\n", PQdb(DbConn));
    printf("User: %s\n", PQuser(DbConn));

    // Check if the table exists
    const char *TableCheckQuery
        = "SELECT EXISTS (SELECT 1 FROM pg_tables WHERE schemaname = 'public' AND tablename = $1)";
    const char *ParamValues[1] = { TableName };
    PGresult   *TableCheckResult
        = PQexecParams(DbConn, TableCheckQuery, 1, NULL, ParamValues, NULL, NULL, 0);

    if(PQresultStatus(TableCheckResult) == PGRES_TUPLES_OK && PQntuples(TableCheckResult) > 0) {
        printf("Table '%s' exists: %s\n", TableName,
               strcmp(PQgetvalue(TableCheckResult, 0, 0), "t") == 0 ? "Yes" : "No");
    } else {
        printf("Error checking table existence: %s\n", PQerrorMessage(DbConn));
    }
    PQclear(TableCheckResult);

    // List all tables in public schema
    const char *ListTablesQuery  = "SELECT tablename FROM pg_tables WHERE schemaname = 'public'";
    PGresult   *ListTablesResult = PQexec(DbConn, ListTablesQuery);

    if(PQresultStatus(ListTablesResult) == PGRES_TUPLES_OK) {
        printf("Tables in public schema:\n");
        for(int i = 0; i < PQntuples(ListTablesResult); i++) {
            printf("  %s\n", PQgetvalue(ListTablesResult, i, 0));
        }
    } else {
        printf("Error listing tables: %s\n", PQerrorMessage(DbConn));
    }
    PQclear(ListTablesResult);
}

void
PrintPGresult(const PGresult *Result)
{
    if(Result == NULL) {
        printf("PGresult is NULL\n");
        return;
    }

    ExecStatusType Status = PQresultStatus(Result);
    printf("Result Status: %s\n", PQresStatus(Status));

    if(Status == PGRES_FATAL_ERROR || Status == PGRES_NONFATAL_ERROR) {
        printf("Error Message: %s\n", PQresultErrorMessage(Result));
    }

    int NumRows = PQntuples(Result);
    int NumCols = PQnfields(Result);
    printf("Number of rows: %d\n", NumRows);
    printf("Number of columns: %d\n", NumCols);

    // Print column names
    printf("Columns:\n");
    for(int Col = 0; Col < NumCols; Col++) {
        printf("  %d: %s\n", Col, PQfname(Result, Col));
    }

    // Print first few rows (up to 5)
    int RowsToPrint = NumRows < 5 ? NumRows : 5;
    printf("First %d rows:\n", RowsToPrint);
    for(int Row = 0; Row < RowsToPrint; Row++) {
        printf("Row %d:\n", Row);
        for(int Col = 0; Col < NumCols; Col++) {
            printf("  %s: %s\n", PQfname(Result, Col), PQgetvalue(Result, Row, Col));
        }
    }

    if(NumRows > 5) {
        printf("... (%d more rows)\n", NumRows - 5);
    }
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

    // PrintPGresult(Result);

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

char *
GetSqlQueryFromFile(const char *FileName, sdb_arena *Arena)
{
    sdb_file_data *File = SdbLoadFileIntoMemory(FileName, Arena);
    return (char *)File->Data;
}
