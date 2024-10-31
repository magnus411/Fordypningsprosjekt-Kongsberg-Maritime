#ifndef POSTGRES_TEST_H
#define POSTGRES_TEST_H
#include <libpq-fe.h>

#include <src/Sdb.h>

#include <src/Common/CircularBuffer.h>
#include <src/Common/SensorDataPipe.h>
#include <src/DatabaseSystems/DatabaseSystems.h>
#include <src/Libs/cJSON/cJSON.h>

typedef struct __attribute__((packed))
{
    i64    Id;
    i64    Timestamp;    // TIMESTAMPTZ
    int    Rpm;          // INTEGER
    double Torque;       // DOUBLE PRECISION
    double Temperature;  // DOUBLE PRECISION
    double Vibration_x;  // DOUBLE PRECISION
    double Vibration_y;  // DOUBLE PRECISION
    double Vibration_z;  // DOUBLE PRECISION
    double Strain;       // DOUBLE PRECISION
    double PowerOutput;  // DOUBLE PRECISION
    double Efficiency;   // DOUBLE PRECISION
    double ShaftAngle;   // DOUBLE PRECISION
    char   SensorId[51]; // VARCHAR(50)

} power_shaft_sensor_data;


void RunPostgresTest(const char *ConfigFilePath);
void TestBinaryInsert(PGconn *DbConnection, const char *TableName, u64 TableNameLen);
void GetSensorSchemasFromDb(PGconn *Connection, sdb_arena *Arena);


sdb_errno PgTestApiInit(database_api *Pg);
sdb_errno PgTestApiRun(database_api *Pg);
sdb_errno PgTestApiFinalize(database_api *Pg);

#endif
