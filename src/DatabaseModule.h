#ifndef DATABASE_MODULE_H_
#define DATABASE_MODULE_H_

#include "SdbExtern.h"

#include <libpq-fe.h>
#include <time.h>

typedef struct
{
    char  *Name;
    Oid    Type;
    size_t Length;
} column_info;

column_info *GetTableStructure(PGconn *DbConn, const char *TableName, int *NCols);
void InsertSensorData(PGconn *DbConn, const char *TableName, u64 TableNameLen, const u8 *SensorData,
                      size_t DataSize);
void TestGetMetadata(PGconn *DbConn, char *MetadataQuery, int *ColCount);
void TestBinaryInsert(PGconn *DbConnection, const char *TableName, u64 TableNameLen);

/* Test stuff */
typedef struct
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
} power_shaft_sensor_data __attribute__((packed));

#endif
