#define SDB_LOG_BUF_SIZE 2048
#define SDB_LOG_LEVEL    4
#include "SdbExtern.h"

SDB_LOG_REGISTER(DatabaseModule);

/*
 * Save to database
 * Read from buffers
 * Monitor database stats
 *   - Ability to prioritize data based on it
 *
 * ? Send messages to comm module?
 * ? Metaprogramming tool to create C structs from sensor schemas ?
 */

#include <libpq-fe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "Postgres.h"
#include "DatabaseModule.h"

column_info *
GetTableStructure(PGconn *DbConn, const char *TableName, int *NCols)
{
#if 0
    char Query[256];
    snprintf(Query, sizeof(Query),
             "SELECT column_name, data_type, pg_catalog.format_type(atttypid, atttypmod) "
             "FROM information_schema.columns "
             "WHERE table_name = '%s' "
             "ORDER BY ordinal_position",
             TableName);
#endif
    PGresult *res
        = PQexecParams(DbConn,
                       "SELECT "
                       "c.column_name, "
                       "c.data_type, "
                       "pg_catalog.format_type(a.atttypid, a.atttypmod) as formatted_type "
                       "FROM "
                       "information_schema.columns c "
                       "JOIN "
                       "pg_catalog.pg_attribute a ON c.column_name = a.attname "
                       "JOIN "
                       "pg_catalog.pg_class cl ON cl.oid = a.attrelid "
                       "WHERE "
                       "c.table_name = $1 "
                       "AND cl.relname = $1 "
                       "ORDER BY "
                       "c.ordinal_position",
                       1,    // one parameter
                       NULL, // let the backend deduce param type
                       &TableName,
                       NULL, // don't need param lengths since text
                       NULL, // default all parameters to text
                       0);   // ask for text results

    if(PQresultStatus(res) != PGRES_TUPLES_OK) {
        SdbLogError("Query failed: %s", PQerrorMessage(DbConn));
        PQclear(res);
        return NULL;
    }

    *NCols               = PQntuples(res);
    column_info *Columns = malloc(*NCols * sizeof(column_info));

    for(int i = 0; i < *NCols; i++) {
        Columns[i].Name      = strdup(PQgetvalue(res, i, 0));
        const char *TypeName = PQgetvalue(res, i, 1);
        const char *FullType = PQgetvalue(res, i, 2);

        // Determine Oid and Length based on Type
        if(strcmp(TypeName, "integer") == 0) {
            Columns[i].Type   = INT4;
            Columns[i].Length = 4;
        } else if(strcmp(TypeName, "bigint") == 0) {
            Columns[i].Type   = BIGINT;
            Columns[i].Length = 8;
        } else if(strcmp(TypeName, "double precision") == 0) {
            Columns[i].Type   = FLOAT8;
            Columns[i].Length = 8;
        } else if(strncmp(TypeName, "character varying", 17) == 0) {
            Columns[i].Type = VARCHAR;
            sscanf(FullType, "character varying(%zd)", &Columns[i].Length);
        } else if(strcmp(TypeName, "timestamp with time zone") == 0) {
            Columns[i].Type   = TIMESTAMPTZ;
            Columns[i].Length = 8;
        } else if(strcmp(TypeName, "text") == 0) {
            Columns[i].Type   = TEXT;
            Columns[i].Length = -1; // variable Length
        } else if(strcmp(TypeName, "boolean") == 0) {
            Columns[i].Type   = BOOL;
            Columns[i].Length = 1;
        } else if(strcmp(TypeName, "date") == 0) {
            Columns[i].Type   = DATE;
            Columns[i].Length = 4;
        } else if(strcmp(TypeName, "time without time zone") == 0) {
            Columns[i].Type   = TIME;
            Columns[i].Length = 8;
        } else if(strcmp(TypeName, "numeric") == 0) {
            Columns[i].Type   = NUMERIC;
            Columns[i].Length = -1; // variable Length
        } else {
            fprintf(stderr, "Unsupported Type: %s\n", TypeName);
            Columns[i].Type   = 0;
            Columns[i].Length = 0;
        } // Determine Oid and Length based on type
    }

    PQclear(res);
    return Columns;
}

void
InsertSensorData(PGconn *DbConn, const char *TableName, const u8 *SensorData, size_t DataSize)
{
    int          NCols;
    column_info *Columns = GetTableStructure(DbConn, TableName, &NCols);
    if(!Columns) {
        return;
    }

    // Construct INSERT query

    char Query[1024] = "INSERT INTO ";
    strcat(Query, TableName);
    strcat(Query, " (");
    for(int i = 0; i < NCols; i++) {
        if(i > 0) {
            strcat(Query, ", ");
        }
        strcat(Query, Columns[i].Name);
    }
    strcat(Query, ") VALUES (");
    for(int i = 0; i < NCols; i++) {
        if(i > 0) {
            strcat(Query, ", ");
        }
        strcat(Query, "$");
        char ParamNumber[8];
        snprintf(ParamNumber, sizeof(ParamNumber), "%d", i + 1);
        strcat(Query, ParamNumber);
    }
    strcat(Query, ")");

    // Prepare the statement
    PGresult *PqPrepareResult = PQprepare(DbConn, "", Query, NCols, NULL);
    if(PQresultStatus(PqPrepareResult) != PGRES_COMMAND_OK) {
        SdbLogError("Prepare failed: %s", PQerrorMessage(DbConn));
        PQclear(PqPrepareResult);
        return;
    }
    PQclear(PqPrepareResult);

    // Set up parameter arrays
    const char **ParamValues  = malloc(NCols * sizeof(char *));
    int         *ParamLengths = malloc(NCols * sizeof(int));
    int         *ParamFormats = malloc(NCols * sizeof(int));

    size_t ParamOffset = 0;
    for(int i = 0; i < NCols; i++) {
        if(ParamOffset + Columns[i].Length > DataSize) {
            SdbLogError("Buffer overflow at column %s\n", Columns[i].Name);
            break;
        }

        ParamValues[i]  = (char *)SensorData + ParamOffset;
        ParamLengths[i] = Columns[i].Length;
        ParamFormats[i] = 1; // Use binary format for all

        // For certain types, we might need to convert to network byte order
        if(Columns[i].Type == 23 || Columns[i].Type == 20 || Columns[i].Type == 701) {
            // Note: This modifies the buffer. If you need to preserve the original,
            // you should work with a copy.
            if(Columns[i].Length == 4) {
                *(uint32_t *)(SensorData + ParamOffset)
                    = htonl(*(uint32_t *)(SensorData + ParamOffset));
            } else if(Columns[i].Length == 8) {
                *(uint64_t *)(SensorData + ParamOffset)
                    = htobe64(*(uint64_t *)(SensorData + ParamOffset));
            }
        }

        ParamOffset += Columns[i].Length;
    }

    // Execute the prepared statement
    PqPrepareResult = PQexecPrepared(DbConn, "", NCols, (const char *const *)ParamValues,
                                     ParamLengths, ParamFormats, 1);

    if(PQresultStatus(PqPrepareResult) != PGRES_COMMAND_OK) {
        SdbLogError("Insert failed: %s", PQerrorMessage(DbConn));
    } else {
        SdbLogDebug("Data inserted successfully\n");
    }

    // Clean up
    PQclear(PqPrepareResult);
    for(int i = 0; i < NCols; i++) {
        free(Columns[i].Name);
    }

    free(Columns);
    free(ParamValues);
    free(ParamLengths);
    free(ParamFormats);
}

void
TestBinaryInsert(PGconn *DbConnection)
{
    // Create an array of test data
    power_shaft_sensor_data SensorData[] = {
        {  .Timestamp   = 0,
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

        { .Timestamp   = 60,
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

        {.Timestamp   = 120,
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

    // Insert each sensor data entry
    for(u64 i = 0; i < sizeof(SensorData) / sizeof(SensorData[0]); i++) {
        InsertSensorData(DbConnection, "power_shaft_sensor", (u8 *)&SensorData[i],
                         sizeof(power_shaft_sensor_data));
        printf("Inserted sensor data entry %lu\n", i + 1);
    }
}

void
PrintColumnMetadata(const pq_col_metadata *Metadata)
{
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
    printf("\n"); // Add a blank line for better readability between entries
}

void
PrintPGresultDebug(const PGresult *Result)
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
TestGetMetadata(PGconn *DbConn, char *MetadataQuery, int *ColCount /* Typical libpq convention that variables for sizes are passed in an filled out by the function */)
{
    PGresult *Result = PQexec(DbConn, MetadataQuery);
    if(PQresultStatus(Result) != PGRES_TUPLES_OK) {
        SdbLogError("Query failed: %s", PQerrorMessage(DbConn));
        PQclear(Result);
    }

    SdbLogDebug("Metadata query successful!");

    PrintPGresultDebug(Result);

    int              RowCount      = PQntuples(Result);
    pq_col_metadata *MetadataArray = malloc(RowCount * sizeof(pq_col_metadata));
    if(!MetadataArray) {
        SdbLogError("Memory allocation failed");
        PQclear(Result);
        return;
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

    *ColCount = RowCount;
    PQclear(Result);

    for(int i = 0; i < *ColCount; ++i) {
        printf("Column %d:\n", i + 1);
        PrintColumnMetadata(&MetadataArray[i]);
    }
}
