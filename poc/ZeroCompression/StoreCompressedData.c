/**
 * @file StoreCompressedData.c
 * @brief Implementation of Compressed Data Storage Utilities
 *
 * Provides concrete implementations for storing metadata about
 * compressed or zero-run data in a PostgreSQL database.
 *
 * Core Functionalities:
 * - Creating database tables for metadata storage
 * - Inserting run-length encoded (RLE) zero data metadata
 * - Storing individual zero data timestamps
 *
 * Storage Strategies:
 * 1. Run-Length Encoding (RLE) Metadata
 * 2. Full Timestamp Metadata

 */


#include <libpq-fe.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <src/Sdb.h>
SDB_LOG_REGISTER(StoreCompressedData);

#include <src/ZeroCompression/StoreCompressedData.h>

//////////////////////////////////////////////////////
//  Storing run length encoded streaks of zero data //
//////////////////////////////////////////////////////


/**
 * @brief Initialize Run-Length Encoded Metadata Storage Table
 *
 * Creates a PostgreSQL table to store RLE zero data metadata if
 * it doesn't already exist.
 *
 * Table Columns:
 * - id: Unique auto-incrementing identifier
 * - start_time: Timestamp marking the beginning of zero data run
 * - end_time: Timestamp marking the end of zero data run
 * - run_length: Number of consecutive zero data points
 *
 * @param Conn Active PostgreSQL database connection
 * @return sdb_errno 0 on successful table creation/existence, -1 on error
 */
sdb_errno
InitializeRLEMetaDataStorage(PGconn *Conn)
{
    const char *CreateTableQuery = "CREATE TABLE IF NOT EXISTS ShaftPowerMetaDataRLEWhenZero ("
                                   "id SERIAL PRIMARY KEY, "
                                   "start_time TIMESTAMP NOT NULL, "
                                   "end_time TIMESTAMP NOT NULL, "
                                   "run_length INT NOT NULL"
                                   ");";

    PGresult *Result = PQexec(Conn, CreateTableQuery);

    if(PQresultStatus(Result) != PGRES_COMMAND_OK) {
        SdbLogError("Error when creating table: %s", PQerrorMessage(Conn));
        PQclear(Result);
        return -1;
    } else {
        SdbLogInfo("Table created or existed. Success.");
        PQclear(Result);
        return 0;
    }
}


/**
 * @brief Store Run-Length Encoded Metadata
 *
 * Inserts a record into the RLE metadata table representing
 * a specific run of zero data points.
 *
 * @param Start Timestamp of zero data run start (string format)
 * @param End Timestamp of zero data run end (string format)
 * @param RunLength Number of consecutive zero data points
 * @param Conn Active PostgreSQL database connection
 * @return sdb_errno 0 on successful insertion, -1 on error
 */
sdb_errno
StoreRLEMetaData(const char *Start, const char *End, size_t RunLength, PGconn *Conn)
{
    const char *ParamValues[3];
    ParamValues[0] = Start;
    ParamValues[1] = End;

    char RunLengthString[16];
    snprintf(RunLengthString, sizeof(RunLengthString), "%zu", RunLength);
    ParamValues[2] = RunLengthString;

    const char *Query = "INSERT INTO ShaftPowerMetaDataRLEWhenZero (start_time, end_time, "
                        "run_length) values ($1, $2, $3);";

    PGresult *Result = PQexecParams(Conn, Query, 3, NULL, ParamValues, NULL, NULL, 0);

    if(PQresultStatus(Result) != PGRES_COMMAND_OK) {
        SdbLogError("Insert into ShaftPowerMetaDataRLEWhenZero failed:\n%s", PQerrorMessage(Conn));
        PQclear(Result);
        return -1;
    } else {
        SdbLogInfo("Insert into ShaftPowerMetaDataRLEWhenZero succeeded.");
        PQclear(Result);
        return 0;
    }
}


/**
 * @brief Store RLE Metadata Using Timespec Structures
 *
 * Similar to StoreRLEMetaData, but accepts high-precision
 * timespec structures for start and end times.
 *
 * Converts timespec to string representations for database storage.
 *
 * @param StartTime High-precision start time
 * @param EndTime High-precision end time
 * @param RunLength Number of consecutive zero data points
 * @param Conn Active PostgreSQL database connection
 * @return sdb_errno 0 on successful insertion, -1 on error
 */
sdb_errno
StoreRLEMetaDataWithTimespecs(const struct timespec *StartTime, const struct timespec *EndTime,
                              size_t RunLength, PGconn *Conn)
{
    // Buffer to hold the string representations of start and end times
    char StartBuffer[32], EndBuffer[32];
    snprintf(StartBuffer, sizeof(StartBuffer), "%ld.%09ld", StartTime->tv_sec, StartTime->tv_nsec);
    snprintf(EndBuffer, sizeof(EndBuffer), "%ld.%09ld", EndTime->tv_sec, EndTime->tv_nsec);

    // Buffer to hold the string representation of run length
    char RunLengthString[16];
    snprintf(RunLengthString, sizeof(RunLengthString), "%zu", RunLength);

    // Prepare the parameter array
    const char *ParamValues[3];
    ParamValues[0] = StartBuffer;
    ParamValues[1] = EndBuffer;
    ParamValues[2] = RunLengthString;

    // SQL query
    const char *Query = "INSERT INTO ShaftPowerMetaDataRLEWhenZero (start_time, end_time, "
                        "run_length) values ($1, $2, $3);";

    // Execute the query
    PGresult *Result = PQexecParams(Conn, Query, 3, NULL, ParamValues, NULL, NULL, 0);

    // Handle the result
    if(PQresultStatus(Result) != PGRES_COMMAND_OK) {
        SdbLogError("Insert into ShaftPowerMetaDataRLEWhenZero failed:\n%s", PQerrorMessage(Conn));
        PQclear(Result);
        return -1;
    } else {
        SdbLogInfo("Insert into ShaftPowerMetaDataRLEWhenZero succeeded.");
        PQclear(Result);
        return 0;
    }
}

//////////////////////////////////////////////////////
// Storing all meta data for instances of zero data //
//////////////////////////////////////////////////////


/**
 * @brief Initialize Full Metadata Storage Table
 *
 * Creates a PostgreSQL table to store individual timestamps
 * of zero data points if it doesn't already exist.
 *
 * Table Columns:
 * - id: Unique auto-incrementing identifier
 * - timestamp: Individual zero data point timestamp
 *
 * @param Conn Active PostgreSQL database connection
 * @return sdb_errno 0 on successful table creation/existence, -1 on error
 */
sdb_errno
InitializeFullMetaDataStorage(PGconn *Conn)
{
    const char *Query = "CREATE TABLE IF NOT EXISTS FullMetaDataWhenZero("
                        "id SERIAL PRIMARY KEY, "
                        "timestamp TIMESTAMP NOT NULL)";

    PGresult *Result = PQexec(Conn, Query);

    if(PQresultStatus(Result) != PGRES_COMMAND_OK) {
        SdbLogError("Error when creating table:\n%s", PQerrorMessage(Conn));
        PQclear(Result);
        return -1;
    } else {
        SdbLogInfo("Creating table succeeded or already existed.");
        PQclear(Result);
        return 0;
    }
}


/**
 * @brief Store Individual Metadata Timestamp
 *
 * Inserts a single timestamp representing a zero data point
 * into the full metadata table.
 *
 * @param TimeStamp Timestamp of the zero data point
 * @param Conn Active PostgreSQL database connection
 * @return sdb_errno 0 on successful insertion, -1 on error
 */
sdb_errno
StoreMetaData(struct timespec *TimeStamp, PGconn *Conn)
{
    const char *Query  = "INSERT INTO FullMetaDataWhenZero (timestamp) Values (%s);";
    PGresult   *Result = PQexec(Conn, Query);

    if(PQresultStatus(Result) != PGRES_COMMAND_OK) {
        SdbLogError("Error when inserting into FullMetaDataWhenZero:\n%s", PQerrorMessage(Conn));
        PQclear(Result);
        return -1;
    } else {
        SdbLogInfo("Insert into FullMetaDataWhenZero succeeded");
        PQclear(Result);
        return 0;
    }
}
