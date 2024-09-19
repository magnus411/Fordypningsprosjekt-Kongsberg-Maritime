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

#include "PostgresTypes.h"
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

        // Determine Oid and Length based on Type
        if(strcmp(TypeName, "integer") == 0)
        {
            Columns[i].Type   = INT4_OID;
            Columns[i].Length = 4;
        }
        else if(strcmp(TypeName, "bigint") == 0)
        {
            Columns[i].Type   = BIGINT_OID;
            Columns[i].Length = 8;
        }
        else if(strcmp(TypeName, "double precision") == 0)
        {
            Columns[i].Type   = FLOAT8_OID;
            Columns[i].Length = 8;
        }
        else if(strncmp(TypeName, "character varying", 17) == 0)
        {
            Columns[i].Type = VARCHAR_OID;
            sscanf(FullType, "character varying(%zd)", &Columns[i].Length);
        }
        else if(strcmp(TypeName, "timestamp with time zone") == 0)
        {
            Columns[i].Type   = TIMESTAMPTZ_OID;
            Columns[i].Length = 8;
        }
        else if(strcmp(TypeName, "text") == 0)
        {
            Columns[i].Type   = TEXT_OID;
            Columns[i].Length = -1; // variable Length
        }
        else if(strcmp(TypeName, "boolean") == 0)
        {
            Columns[i].Type   = BOOL_OID;
            Columns[i].Length = 1;
        }
        else if(strcmp(TypeName, "date") == 0)
        {
            Columns[i].Type   = DATE_OID;
            Columns[i].Length = 4;
        }
        else if(strcmp(TypeName, "time without time zone") == 0)
        {
            Columns[i].Type   = TIME_OID;
            Columns[i].Length = 8;
        }
        else if(strcmp(TypeName, "numeric") == 0)
        {
            Columns[i].Type   = NUMERIC_OID;
            Columns[i].Length = -1; // variable Length
        }
        else
        {
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
    for(u64 i = 0; i < sizeof(SensorData) / sizeof(SensorData[0]); i++)
    {
        InsertSensorData(DbConnection, "power_shaft_sensor", (u8 *)&SensorData[i],
                         sizeof(power_shaft_sensor_data));
        printf("Inserted sensor data entry %lu\n", i + 1);
    }
}
