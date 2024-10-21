#include <src/Sdb.h>
SDB_LOG_REGISTER(DbSystems);

#include <src/Common/SensorDataPipe.h>
#include <src/DatabaseSystems/DatabaseSystems.h>
#include <src/Modules/DatabaseModule.h>

#include <tests/DatabaseSystems/PostgresTest.h>

sdb_errno
DbsInitApiTest(Db_System_Type Type, sensor_data_pipe *SdPipe, sdb_arena *Arena, u64 ArenaSize,
               i64 DbmTId, database_api *DbsApi)
{
    if(!DbsDatabaseIsAvailable(Type)) {
        SdbLogWarning("%s is unavailable", DbsTypeToName(Type));
        return -SDBE_DBS_UNAVAIL;
    }

    SdbMemset(DbsApi, 0, sizeof(*DbsApi));
    SdbMemcpy(&DbsApi->SdPipe, SdPipe, sizeof(*SdPipe));
    SdbArenaBootstrap(Arena, &DbsApi->Arena, ArenaSize);

    switch(Type) {
        case Dbs_Postgres:
            {
                DbsApi->Init     = PgInitTest;
                DbsApi->Run      = PgRunTest;
                DbsApi->Finalize = PgFinalizeTest;
            }
            break;
        default:
            {
                SdbAssert(0, "Database type doesn't exist");
                return -EINVAL;
            }
            break;
    }

    return 0;
}
