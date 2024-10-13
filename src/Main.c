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

int
main(int ArgCount, char **ArgV)
{
    sdb_arena SdbArena;
    u64       SdbArenaSize = SdbMebiByte(32);
    u8       *SdbArenaMem  = malloc(SdbArenaSize);

    if(NULL == SdbArenaMem) {
        SdbLogError("Failed to allocate memory for arena!");
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
    DbModuleCtx->ThreadId      = (i64)Dbs_Postgres;
    // TODO(ingar): Is it a good idea to have the thread id match the database id?

    SdbArenaBootstrap(&SdbArena, &DbModuleCtx->Arena, SdbMebiByte(9));
    SdbMemcpy(&DbModuleCtx->SdPipe, SdPipe, sizeof(sensor_data_pipe));

    pthread_t DbThread;
    pthread_create(&DbThread, NULL, DbModuleRun, DbModuleCtx);
    pthread_join(DbThread, NULL);

    exit(EXIT_SUCCESS);
}
