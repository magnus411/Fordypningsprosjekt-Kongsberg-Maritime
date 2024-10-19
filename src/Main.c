
#include <libpq-fe.h>
#include <pthread.h>
#include <stdlib.h>

#define SDB_H_IMPLEMENTATION
#include <src/Sdb.h>
#undef SDB_H_IMPLEMENTATION

SDB_LOG_REGISTER(Main);

#include <src/Common/SensorDataPipe.h>
#include <src/DatabaseSystems/DatabaseInitializer.h>
#include <src/DatabaseSystems/DatabaseSystems.h>
#include <src/Modules/CommModule.h>
#include <src/Modules/DatabaseModule.h>

#define SD_PIPE_BUF_COUNT 4

// TODO(ingar): Is it fine to keep this globally?
static i64 NextDbmTId_  = 1;
static i64 NextCommTId_ = 1;

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
    DbModuleCtx->ThreadId      = NextDbmTId_++;
    DbModuleCtx->DbsType       = Dbs_Postgres;
    DbModuleCtx->ArenaSize     = SdbMebiByte(9);
    DbModuleCtx->DbsArenaSize  = SdbMebiByte(8);
    SdbArenaBootstrap(&SdbArena, &DbModuleCtx->Arena, DbModuleCtx->ArenaSize);
    SdbMemcpy(&DbModuleCtx->SdPipe, SdPipe, sizeof(*SdPipe));

    comm_module_ctx *CommModuleCtx = SdbPushStruct(&SdbArena, comm_module_ctx);
    CommModuleCtx->ThreadId        = NextCommTId_++;
    CommModuleCtx->CpType          = Comm_Protocol_Modbus_TCP;
    CommModuleCtx->ArenaSize       = SdbMebiByte(9);
    CommModuleCtx->CpArenaSize     = SdbMebiByte(8);
    SdbArenaBootstrap(&SdbArena, &CommModuleCtx->Arena, CommModuleCtx->ArenaSize);
    SdbMemcpy(&CommModuleCtx->SdPipe, SdPipe, sizeof(*SdPipe));

    pthread_t DbmThread;
    pthread_t CommThread;
    pthread_create(&DbmThread, NULL, DbModuleRun, DbModuleCtx);
    pthread_create(&CommThread, NULL, CommModuleRun, CommModuleCtx);
    pthread_join(CommThread, NULL);
    pthread_join(DbmThread, NULL);

    if(DbModuleCtx->Errno == 0 && CommModuleCtx->Errno == 0) {
        exit(EXIT_SUCCESS);
    } else {
        exit(EXIT_FAILURE);
    }
}
