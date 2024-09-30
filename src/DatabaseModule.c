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

#define SDB_LOG_LEVEL 4
#include "SdbExtern.h"

#include "Postgres.h"
#include "DatabaseModule.h"

SDB_LOG_REGISTER(DatabaseModule);

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

    // Construct INSERT query
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

    // Prepare the statement
    PGresult *PqPrepareResult = PQprepare(DbConn, "", Query, NTableCols, NULL);
    if(PQresultStatus(PqPrepareResult) != PGRES_COMMAND_OK) {
        SdbLogError("Prepare failed: %s", PQerrorMessage(DbConn));
        PQclear(PqPrepareResult);
        return;
    }
    PQclear(PqPrepareResult);

    // Set up parameter arrays
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

        // NOTE(ingar): Convert to network order
        if(ColMetadata[i].TypeOid == INT4 || ColMetadata[i].TypeOid == INT8
           || ColMetadata[i].TypeOid == FLOAT8) {
            // Note(ingar): This modifies the buffer.
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
            ParamFormats[i]         = 0; // NOTE(ingar): Set to use text format instead of binary

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
