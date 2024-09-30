#include <libpq-fe.h>

#include "../SdbExtern.h"

/* Test stuff */
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

typedef struct
{
    int   Id;
    char *Name;
    int   SampleRate;
    char *Variables;
} sensor_schema;

void TestGetMetadata(PGconn *DbConn, char *MetadataQuery, int *ColCount);

void TestBinaryInsert(PGconn *DbConnection, const char *TableName, u64 TableNameLen);
