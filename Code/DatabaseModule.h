#ifndef DATABASE_MODULE_H_
#define DATABASE_MODULE_H_

#include "SdbExtern.h"

#include <libpq-fe.h>

typedef struct
{
    char  *Name;
    Oid    Type;
    size_t Length;
} column_info;

column_info *GetTableStructure(PGconn *DbConn, const char *TableName, int *NCols);

void InsertSensorData(PGconn *DbConn, const char *TableName, const u8 *SensorData,
                      size_t BufferSize);

#endif
