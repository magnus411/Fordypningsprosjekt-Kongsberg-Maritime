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
DbsDatabaseIsAvailable(Db_System_Id DbsId)
{
    switch(DbsId) {
        case Dbs_Postgres:
            return PgIsAvailable_;
        default:
            return false;
    }
}

sdb_errno
DbsInitApi(Db_System_Id DbsId, sensor_data_pipe *SdPipe, sdb_arena *Arena, u64 ArenaSize,
           database_api *Dbs)
{
    switch(DbsId) {
        case Dbs_Postgres:
            {
                if(!DbsDatabaseIsAvailable(DbsId)) {
                    SdbLogWarning("PostgreSQL is unavailable");
                    return -SDBE_DBS_UNAVAIL;
                }
                Dbs->Init     = PgInit;
                Dbs->Run      = PgRun;
                Dbs->Finalize = PgFinalize;
            }
            break;
        default:
            {
                SdbLogError("Database id doesn't exist");
                return -EINVAL;
            }
            break;
    }

    SdbMemcpy(&Dbs->SdPipe, SdPipe, sizeof(sensor_data_pipe));
    SdbArenaBootstrap(Arena, &Dbs->Arena, ArenaSize);

    return 0;
}
