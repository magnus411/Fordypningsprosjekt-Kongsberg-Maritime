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

#include "DatabaseModule.h"

column_info *
get_table_structure(PGconn *DbConn, const char *TableName, int *NCols)
{
    char query[256];
    snprintf(query, sizeof(query),
             "SELECT column_name, data_type, pg_catalog.format_type(atttypid, atttypmod) "
             "FROM information_schema.columns "
             "WHERE table_name = '%s' "
             "ORDER BY ordinal_position",
             TableName);

    PGresult *res = PQexec(DbConn, query);
    if(PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        SdbLogError("Query failed: %s", PQerrorMessage(DbConn));
        PQclear(res);
        return NULL;
    }

    *NCols               = PQntuples(res);
    column_info *Columns = malloc(*NCols * sizeof(column_info));

    for(int i = 0; i < *NCols; i++)
    {
        Columns[i].Name      = strdup(PQgetvalue(res, i, 0));
        const char *TypeName = PQgetvalue(res, i, 1);
        const char *FullType = PQgetvalue(res, i, 2);

        // Determine Oid and length based on type
        if(strcmp(TypeName, "integer") == 0)
        {
            Columns[i].Type   = 23; // INT4OID
            Columns[i].Length = 4;
        }
        else if(strcmp(TypeName, "bigint") == 0)
        {
            Columns[i].Type   = 20; // INT8OID
            Columns[i].Length = 8;
        }
        else if(strcmp(TypeName, "double precision") == 0)
        {
            Columns[i].Type   = 701; // FLOAT8OID
            Columns[i].Length = 8;
        }
        else if(strncmp(TypeName, "character varying", 17) == 0)
        {
            Columns[i].Type = 1043; // VARCHAROID
            sscanf(FullType, "character varying(%zd)", &Columns[i].Length);
        }
        else if(strcmp(TypeName, "timestamp with time zone") == 0)
        {
            Columns[i].Type   = 1184; // TIMESTAMPTZOID
            Columns[i].Length = 8;
        }
        else
        {
            SdbLogError("Unsupported type: %s\n", TypeName);
            Columns[i].Type   = 0;
            Columns[i].Length = 0;
        }
    }

    PQclear(res);
    return Columns;
}

void
InsertSensorData(PGconn *DbConn, const char *TableName, const u8 *SensorData, size_t DataSize)
{
    int          NCols;
    column_info *Columns = get_table_structure(DbConn, TableName, &NCols);
    if(!Columns)
    {
        return;
    }

    // Construct INSERT query

    char Query[1024] = "INSERT INTO ";
    strcat(Query, TableName);
    strcat(Query, " (");
    for(int i = 0; i < NCols; i++)
    {
        if(i > 0)
        {
            strcat(Query, ", ");
        }
        strcat(Query, Columns[i].Name);
    }
    strcat(Query, ") VALUES (");
    for(int i = 0; i < NCols; i++)
    {
        if(i > 0)
        {
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
    if(PQresultStatus(PqPrepareResult) != PGRES_COMMAND_OK)
    {
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
    for(int i = 0; i < NCols; i++)
    {
        if(ParamOffset + Columns[i].Length > DataSize)
        {
            SdbLogError("Buffer overflow at column %s\n", Columns[i].Name);
            break;
        }

        ParamValues[i]  = (char *)SensorData + ParamOffset;
        ParamLengths[i] = Columns[i].Length;
        ParamFormats[i] = 1; // Use binary format for all

        // For certain types, we might need to convert to network byte order
        if(Columns[i].Type == 23 || Columns[i].Type == 20 || Columns[i].Type == 701)
        {
            // Note: This modifies the buffer. If you need to preserve the original,
            // you should work with a copy.
            if(Columns[i].Length == 4)
            {
                *(uint32_t *)(SensorData + ParamOffset)
                    = htonl(*(uint32_t *)(SensorData + ParamOffset));
            }
            else if(Columns[i].Length == 8)
            {
                *(uint64_t *)(SensorData + ParamOffset)
                    = htobe64(*(uint64_t *)(SensorData + ParamOffset));
            }
        }

        ParamOffset += Columns[i].Length;
    }

    // Execute the prepared statement
    PqPrepareResult = PQexecPrepared(DbConn, "", NCols, (const char *const *)ParamValues,
                                     ParamLengths, ParamFormats, 1);

    if(PQresultStatus(PqPrepareResult) != PGRES_COMMAND_OK)
    {
        SdbLogError("Insert failed: %s", PQerrorMessage(DbConn));
    }
    else
    {
        SdbLogDebug("Data inserted successfully\n");
    }

    // Clean up
    PQclear(PqPrepareResult);
    for(int i = 0; i < NCols; i++)
    {
        free(Columns[i].Name);
    }
    free(Columns);
    free(ParamValues);
    free(ParamLengths);
    free(ParamFormats);
}

bool
TestBinaryInsert(PGconn *DbConnection)
{
    // Simulated sensor data buffer
    unsigned char buffer[] = {
        0x00, 0x00, 0x00, 0x01,                         // integer: 1
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A, // bigint: 10
        0x40, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // double: 42.5
        0x53, 0x45, 0x4E, 0x53, 0x30, 0x30, 0x31, 0x00  // char(8): "SENS001"
    };

    InsertSensorData(DbConnection, "your_sensor_table", buffer, sizeof(buffer));

    return 0;
}
