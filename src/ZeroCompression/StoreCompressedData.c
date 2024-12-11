#include <src/Sdb.h>
#include <time.h>
SDB_LOG_REGISTER(StoreCompressedData);
#include <libpq-fe.h>
#include <src/StoreCompressedData.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//////////////////////////////////////////////////////
//  Storing run length encoded streaks of zero data //
//////////////////////////////////////////////////////

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
