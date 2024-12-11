#include <src/Sdb.h>
SDB_LOG_REGISTER(MbWithPg);

#include <src/Common/SensorDataPipe.h>
#include <src/Common/ThreadGroup.h>
#include <src/DataHandlers/DataHandlers.h>
#include <src/DataHandlers/ModbusWithPostgres/Modbus.h>
#include <src/DataHandlers/ModbusWithPostgres/ModbusWithPostgres.h>
#include <src/DataHandlers/ModbusWithPostgres/Postgres.h>

#include <src/Libs/cJSON/cJSON.h>


void *
PgThread(void *Arg)
{
    sdb_errno Ret = PgRun(Arg);
    if(Ret != 0) {
        SdbLogError("Postgres thread exited with error code %d (%s)", Ret, SdbStrErr(Ret));
    }
    return NULL;
}

void *
MbThread(void *Arg)
{
    sdb_errno Ret = MbRun(Arg);
    if(Ret != 0) {
        SdbLogError("Modbus thread exited with error code %d (%s)", Ret, SdbStrErr(Ret));
    }
    return NULL;
}

void *
MbPgTestServer(void *Arg)
{
    return NULL;
}

static void
GetMemAndScratchSize(cJSON *Conf, u64 *MemSize, u64 *ScratchSize)
{
    cJSON *MemSizeObj = cJSON_GetObjectItem(Conf, "mem");
    if(!cJSON_IsString(Conf)) {
        return;
    }

    cJSON *ScratchSizeObj = cJSON_GetObjectItem(Conf, "scratch_size");
    if(!cJSON_IsString(Conf)) {
        return;
    }

    *MemSize     = SdbMemSizeFromString(cJSON_GetStringValue(MemSizeObj));
    *ScratchSize = SdbMemSizeFromString(cJSON_GetStringValue(ScratchSizeObj));
}

sdb_errno
MbPgCleanup(void *Arg)
{
    mbpg_ctx *Ctx = Arg;
    if(Ctx) {
        if(Ctx->SdPipe) {
            SdpDestroy(Ctx->SdPipe, false);
        } else {
            SdbLogWarning("The context passed to cleanup function's pipe was NULL");
            return -SDBE_PTR_WAS_NULL;
        }
        SdbBarrierDeinit(&Ctx->Barrier);
        free(Ctx);
    } else {
        SdbLogWarning("The context passed to cleanup function was NULL");
        return -SDBE_PTR_WAS_NULL;
    }

    return 0;
}


static tg_task MbPgTasks[]     = { PgThread, MbThread };
static tg_task MbPgTestTasks[] = { PgThread, MbThread, MbPgTestServer };

tg_group *
MbPgCreateTg(cJSON *Conf, u64 GroupId, sdb_arena *A)
{
    cJSON *ModbusConf   = cJSON_GetObjectItem(Conf, "modbus");
    cJSON *PostgresConf = cJSON_GetObjectItem(Conf, "postgres");
    cJSON *PipeConf     = cJSON_GetObjectItem(Conf, "pipe");
    cJSON *TestConf     = cJSON_GetObjectItem(Conf, "testing");

    mbpg_ctx *Ctx = malloc(sizeof(mbpg_ctx));
    if(Ctx == NULL) {
        return NULL;
    }

    cJSON *PipeBufCountObj = cJSON_GetObjectItem(PipeConf, "buf_count");
    cJSON *PipeBufSizeObj  = cJSON_GetObjectItem(PipeConf, "buf_size");

    DhsGetMemAndScratchSize(ModbusConf, &Ctx->ModbusMemSize, &Ctx->ModbusScratchSize);
    DhsGetMemAndScratchSize(PostgresConf, &Ctx->PgMemSize, &Ctx->PgScratchSize);

    u64 PipeBufCount = cJSON_GetNumberValue(PipeBufCountObj);
    u64 PipeBufSize  = SdbMemSizeFromString(cJSON_GetStringValue(PipeBufSizeObj));
    Ctx->SdPipe      = SdpCreate(PipeBufCount, PipeBufSize, NULL);
    if(Ctx->SdPipe == NULL) {
        free(Ctx);
        return NULL;
    }

    tg_group *Group;
    cJSON    *TestingEnabled = cJSON_GetObjectItem(TestConf, "enabled");
    if(cJSON_IsTrue(TestingEnabled)) {
        SdbBarrierInit(&Ctx->Barrier, 3);
        Group = TgCreateGroup(GroupId, 3, Ctx, NULL, MbPgTestTasks, MbPgCleanup, A);
    } else {
        Group = TgCreateGroup(GroupId, 2, Ctx, NULL, MbPgTasks, MbPgCleanup, A);
        SdbBarrierInit(&Ctx->Barrier, 2);
    }

    return Group;
}
