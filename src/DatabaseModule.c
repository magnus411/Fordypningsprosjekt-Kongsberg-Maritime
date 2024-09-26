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
 * ? Send messages to comm module ?
 * ? Metaprogramming tool to create C structs from sensor schemas ?
 */

#include <libpq-fe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <iconv.h>

#include "Postgres.h"
#include "DatabaseModule.h"

char *
convert_encoding(const char *input, const char *from_charset, const char *to_charset)
{
    iconv_t cd = iconv_open(to_charset, from_charset);
    if(cd == (iconv_t)-1) {
        // Handle error
        return NULL;
    }

    size_t inbytesleft  = strlen(input);
    size_t outbytesleft = inbytesleft * 4; // Allocate more space for UTF-8
    char  *output       = malloc(outbytesleft);
    char  *outbuf       = output;

    if(iconv(cd, &input, &inbytesleft, &outbuf, &outbytesleft) == (size_t)-1) {
        // Handle error
        free(output);
        iconv_close(cd);
        return NULL;
    }

    iconv_close(cd);
    return output;
}

void
InsertSensorData(PGconn *DbConn, const char *TableName, u64 TableNameLen, const u8 *SensorData,
                 size_t DataSize)
{
    int              NTableCols;
    pq_col_metadata *ColMetadata = GetTableMetadata(DbConn, TableName, TableNameLen, &NTableCols);
    if(NULL == ColMetadata) {
        return;
    }
    if(0 == NTableCols) {
        SdbLogError("Table had no columns!");
        return;
    }

    // Construct INSERT query

    char Query[1024] = "INSERT INTO ";
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
        snprintf(ParamNumber, sizeof(ParamNumber), "%d", i + 1);
        strcat(Query, ParamNumber);
    }
    strcat(Query, ")");

    SdbLogDebug("%s", Query);

    // Prepare the statement
    PGresult *PqPrepareResult = PQprepare(DbConn, "", Query, NTableCols, NULL);
    if(PQresultStatus(PqPrepareResult) != PGRES_COMMAND_OK) {
        SdbLogError("Prepare failed: %s", PQerrorMessage(DbConn));
        PQclear(PqPrepareResult);
        return;
    }
    PQclear(PqPrepareResult);

    // Set up parameter arrays
    const char **ParamValues  = malloc(NTableCols * sizeof(char *));
    int         *ParamLengths = malloc(NTableCols * sizeof(int));
    int         *ParamFormats = malloc(NTableCols * sizeof(int));

    int    AllocedString = 0;
    char **AllocedStrings;
    size_t ParamOffset = 0;
    for(int i = 0; i < NTableCols; i++) {
        if(ParamOffset + ColMetadata[i].TypeLength > DataSize) {
            SdbLogError("Buffer overflow at column %s\n", ColMetadata[i].ColumnName);
            break;
        }

        ParamValues[i]  = (char *)SensorData + ParamOffset;
        ParamLengths[i] = ColMetadata[i].TypeLength;
        ParamFormats[i] = 1; // Use binary format for all

        // For certain types, we might need to convert to network byte order
        if(ColMetadata[i].TypeOid == INT4 || ColMetadata[i].TypeOid == INT8
           || ColMetadata[i].TypeOid == FLOAT8) {
            // Note: This modifies the buffer. If you need to preserve the original,
            // you should work with a copy.
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
            char *utf8_string       = convert_encoding(StringStart, "ISO-8859-1", "UTF-8");

            SdbLogDebug("Attempting to inser %ldu chars into varchar", ParamLengths[i]);
        }

        ParamOffset += ColMetadata[i].TypeLength;
        SdbLogDebug("Insert %d: ParamValue %p, ParamLength %d, ParamFormat %d", i, ParamValues[i],
                    ParamLengths[i], ParamFormats[i]);
    }

    // Execute the prepared statement
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

void
TestBinaryInsert(PGconn *DbConnection, const char *TableName, u64 TableNameLen)
{
    // Create an array of test data
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
