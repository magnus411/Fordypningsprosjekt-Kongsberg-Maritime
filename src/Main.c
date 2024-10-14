#include <libpq-fe.h>
#include <pthread.h>
#include <stdlib.h>

#define SDB_H_IMPLEMENTATION
#include <src/Sdb.h>
#undef SDB_H_IMPLEMENTATION

SDB_LOG_REGISTER(Main);

#include <src/Common/SensorDataPipe.h>
#include <src/DatabaseSystems/DatabaseSystems.h>
#include <src/Modules/DatabaseModule.h>

#define SD_PIPE_BUF_COUNT 4

// TODO(ingar): Is it fine to keep this globally?
static i64 NextDbmTId_ = 1;

int
main(int ArgCount, char **ArgV)
{
    sdb_arena SdbArena;
    u64       SdbArenaSize = SdbMebiByte(32);
    u8       *SdbArenaMem  = malloc(SdbArenaSize);

    if(NULL == SdbArenaMem) {
        SdbLogError("Failed to allocate memory for arena");
        exit(EXIT_FAILURE);
    } else {
        SdbArenaInit(&SdbArena, SdbArenaMem, SdbArenaSize);
    }

    size_t BufSizes[SD_PIPE_BUF_COUNT]
        = { SdbKibiByte(32), SdbKibiByte(32), SdbKibiByte(32), SdbKibiByte(32) };

    sensor_data_pipe *SdPipe      = SdbPushStruct(&SdbArena, sensor_data_pipe);
    sdb_errno         PipeInitRet = SdPipeInit(SdPipe, SD_PIPE_BUF_COUNT, BufSizes, &SdbArena);
    if(PipeInitRet != 0) {
        SdbLogError("Failed to init sensor data pipe");
        exit(EXIT_FAILURE);
    }

    db_module_ctx *DbModuleCtx = SdbPushStruct(&SdbArena, db_module_ctx);
    DbModuleCtx->DbsToRun      = Dbs_Postgres;
    DbModuleCtx->ThreadId      = NextDbmTId_++;

    SdbArenaBootstrap(&SdbArena, &DbModuleCtx->Arena, SdbMebiByte(9));
    SdbMemcpy(&DbModuleCtx->SdPipe, SdPipe, sizeof(sensor_data_pipe));

    pthread_t DbThread;
    pthread_create(&DbThread, NULL, DbModuleRun, DbModuleCtx);
    pthread_join(DbThread, NULL);

    if(DbModuleCtx->Errno != 0) {
        SdbLogError("Database module thread %ld, running database %s, finished with error: %d",
                    DbModuleCtx->ThreadId, DbsIdToName(DbModuleCtx->DbsToRun), DbModuleCtx->Errno);
        exit(EXIT_FAILURE);
    } else {
        SdbLogInfo("Database module thread %ld, running database %s, finished successfully",
                   DbModuleCtx->ThreadId, DbsIdToName(DbModuleCtx->DbsToRun));
        exit(EXIT_SUCCESS);
    }
}
