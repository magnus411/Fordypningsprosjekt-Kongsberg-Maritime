#include <stdlib.h>

#define SDB_H_IMPLEMENTATION
#include <src/Sdb.h>
#undef SDB_H_IMPLEMENTATION

SDB_LOG_REGISTER(MainTests);

#include <src/Common/SensorDataPipe.h>
#include <src/Common/Thread.h>
#include <src/DatabaseSystems/DatabaseSystems.h>
#include <src/Modules/CommModule.h>
#include <src/Modules/DatabaseModule.h>

#include <tests/CommProtocols/CommProtocolsTest.h>
#include <tests/CommProtocols/ModbusTest.h>
#include <tests/DatabaseSystems/DatabaseSystemsTest.h>

#define SD_PIPE_BUF_COUNT 4

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


    const u32   ThreadCount = 3;
    sdb_barrier ModulesBarrier;
    SdbBarrierInit(&ModulesBarrier, ThreadCount);
    // NOTE(ingar): This is used to ensure that all modules have been initialized before starting to
    // read from the pipe


    db_module_ctx *DbmCtx  = SdbPushStruct(&SdbArena, db_module_ctx);
    DbmCtx->ModulesBarrier = &ModulesBarrier;
    DbmCtx->DbsType        = Dbs_Postgres;
    DbmCtx->InitApi        = DbsInitApiTest;
    DbmCtx->ArenaSize      = SdbMebiByte(9);
    DbmCtx->DbsArenaSize   = SdbMebiByte(8);

    SdbArenaBootstrap(&SdbArena, &DbmCtx->Arena, DbmCtx->ArenaSize);
    SdbMemcpy(&DbmCtx->SdPipe, SdPipe, sizeof(*SdPipe));


    comm_module_ctx *CommCtx = SdbPushStruct(&SdbArena, comm_module_ctx);
    CommCtx->ModulesBarrier  = &ModulesBarrier;
    CommCtx->CpType          = Comm_Protocol_Modbus_TCP;
    CommCtx->InitApi         = CpInitApiTest;
    CommCtx->ArenaSize       = SdbMebiByte(9);
    CommCtx->CpArenaSize     = SdbMebiByte(8);

    SdbArenaBootstrap(&SdbArena, &CommCtx->Arena, CommCtx->ArenaSize);
    SdbMemcpy(&CommCtx->SdPipe, SdPipe, sizeof(*SdPipe));


    sdb_thread DbmThread, CommThread, ModbusServerThread;
    SdbThreadCreate(&ModbusServerThread, RunModbusTestServer, &ModulesBarrier);
    SdbThreadCreate(&DbmThread, DbModuleRun, DbmCtx);
    SdbThreadCreate(&CommThread, CommModuleRun, CommCtx);

    sdb_errno ModbusRet = SdbThreadJoin(&ModbusServerThread);
    sdb_errno DbmRet    = SdbThreadJoin(&DbmThread);
    sdb_errno CommRet   = SdbThreadJoin(&CommThread);


    if(DbmRet == 0 && CommRet == 0 && ModbusRet == 0) {
        exit(EXIT_SUCCESS);
    } else {
        exit(EXIT_FAILURE);
    }
}
