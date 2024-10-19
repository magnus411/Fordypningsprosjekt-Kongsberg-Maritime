#include <src/Sdb.h>
SDB_LOG_REGISTER(DbSystems);

#include <src/Common/SdbErrno.h>
#include <src/Common/SensorDataPipe.h>
#include <src/DatabaseSystems/DatabaseSystems.h>
#include <src/DatabaseSystems/Postgres.h>
#include <src/Modules/DatabaseModule.h>

#if DATABASE_SYSTEM_POSTGRES == 1
static bool PgIsAvailable_ = true;
#else
static bool PgApiIsAvailable_ = false;
#endif // DATABASE_SYSTEM_POSTGRES

bool
DbsDatabaseIsAvailable(Db_System_Type DbsId)
{
    switch(DbsId) {
        case Dbs_Postgres:
            return PgIsAvailable_;
        default:
            return false;
    }
}

sdb_errno
DbsInitApi(Db_System_Type Type, sensor_data_pipe *SdPipe, sdb_arena *Arena, u64 ArenaSize,
           i64 DbmTId, database_api *DbsApi, void *OptArgs)
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
                DbsApi->Init     = PgInit;
                DbsApi->Run      = PgRun;
                DbsApi->Finalize = PgFinalize;
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
