#include <src/Sdb.h>
SDB_LOG_REGISTER(DbSystemsTest);

#include <src/Common/SensorDataPipe.h>
#include <src/DatabaseSystems/DatabaseSystems.h>

#include <tests/DatabaseSystems/PostgresTest.h>

sdb_errno
DbsTestApiInit(Db_System_Type Type, sdb_thread_control *ModuleControl, u64 SensorCount,
               sensor_data_pipe **Pipes, sdb_arena *Arena, u64 ArenaSize, i64 DbmTId,
               database_api *DbsApi)
{
    if(!DbsDatabaseIsAvailable(Type)) {
        SdbLogWarning("%s is unavailable", DbsTypeToName(Type));
        return -SDBE_DBS_UNAVAIL;
    }

    SdbMemset(DbsApi, 0, sizeof(*DbsApi));
    DbsApi->ModuleControl = ModuleControl;
    DbsApi->SensorCount   = SensorCount;
    DbsApi->SdPipes       = Pipes;
    SdbArenaBootstrap(Arena, &DbsApi->Arena, ArenaSize);

    switch(Type) {
        case Dbs_Postgres:
            {
                DbsApi->Init     = PgTestApiInit;
                DbsApi->Run      = PgTestApiRun;
                DbsApi->Finalize = PgTestApiFinalize;
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
