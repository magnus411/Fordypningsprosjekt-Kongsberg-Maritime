#include <src/Sdb.h>
SDB_LOG_REGISTER(DbSystemsTest);

#include <src/Common/SensorDataPipe.h>
#include <src/DatabaseSystems/DatabaseSystems.h>

#include <tests/DatabaseSystems/PostgresTest.h>

sdb_errno
DbsInitApiTest(Db_System_Type Type, u64 SensorCount, sensor_data_pipe **Pipes, sdb_arena *Arena,
               u64 ArenaSize, i64 DbmTId, database_api *DbsApi)
{
    if(!DbsDatabaseIsAvailable(Type)) {
        SdbLogWarning("%s is unavailable", DbsTypeToName(Type));
        return -SDBE_DBS_UNAVAIL;
    }

    SdbMemset(DbsApi, 0, sizeof(*DbsApi));
    DbsApi->SensorCount = SensorCount;
    DbsApi->SdPipes     = Pipes;
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
