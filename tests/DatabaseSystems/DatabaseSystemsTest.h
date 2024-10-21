#ifndef DATABASE_SYSTEMS_TEST_H
#define DATABASE_SYSTEMS_TEST_H

#include <src/Sdb.h>

#include <src/Common/SensorDataPipe.h>
#include <src/DatabaseSystems/DatabaseSystems.h>
#include <src/Libs/cJSON/cJSON.h>

sdb_errno DbsInitApiTest(Db_System_Type DbsType, sensor_data_pipe *SdPipe, sdb_arena *Arena,
                         u64 ArenaSize, i64 DbmTId, database_api *Dbs);

#endif
