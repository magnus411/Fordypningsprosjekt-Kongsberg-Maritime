#include <src/Sdb.h>
SDB_LOG_REGISTER(MbWithPg);

#include <src/Common/SensorDataPipe.h>
#include <src/Common/ThreadGroup.h>

#include <src/Libs/cJSON/cJSON.h>

#include "Modbus.h"
#include "ModbusWithPostgres.h"
#include "Postgres.h"

void *
PgThread(void *Arg)
{
    PgRun(Arg);

    return NULL;
}

void *
MbThread(void *Arg)
{
    MbRun(Arg);
    return NULL;
}

// TODO(ingar): Move to threadgroup
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

void *
MbPgInit(cJSON *Conf)
{
    cJSON *ModbusConf = cJSON_GetObjectItem(Conf, "modbus");
    if(ModbusConf == NULL || !cJSON_IsObject(ModbusConf)) {
        return NULL;
    }

    cJSON *PostgresConf = cJSON_GetObjectItem(Conf, "postgres");
    if(PostgresConf == NULL || !cJSON_IsObject(PostgresConf)) {
        return NULL;
    }

    cJSON *PipeConf = cJSON_GetObjectItem(Conf, "pipe");
    if(PipeConf == NULL || !cJSON_IsObject(PipeConf)) {
        return NULL;
    }

    cJSON *TestingEnabled;
    cJSON *TestConf = cJSON_GetObjectItem(Conf, "testing");
    if(TestConf == NULL || !cJSON_IsObject(TestConf)) {
        return NULL;
    } else {
        // TODO(ingar): Implement how testing is done
        TestingEnabled = cJSON_GetObjectItem(Conf, "enabled");
    }


    cJSON *PipeBufCountObj = cJSON_GetObjectItem(PipeConf, "buf_count");
    cJSON *PipeBufSizeObj  = cJSON_GetObjectItem(PipeConf, "buf_size");
    if(!PipeBufCountObj || !PipeBufSizeObj) {
        return NULL;
    }

    mbpg_ctx *Ctx = malloc(sizeof(mbpg_ctx));
    if(Ctx == NULL) {
        return NULL;
    }

    GetMemAndScratchSize(ModbusConf, &Ctx->ModbusMemSize, &Ctx->ModbusScratchSize);
    GetMemAndScratchSize(PostgresConf, &Ctx->PgMemSize, &Ctx->PgScratchSize);

    u64 PipeBufCount = cJSON_GetNumberValue(PipeBufCountObj);
    u64 PipeBufSize  = SdbMemSizeFromString(cJSON_GetStringValue(PipeBufSizeObj));
    Ctx->SdPipe      = SdpCreate(PipeBufCount, PipeBufSize, NULL);
    if(Ctx->SdPipe == NULL) {
        free(Ctx);
        return NULL;
    }

    return Ctx;
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
        }
        free(Ctx);
        return -SDBE_PTR_WAS_NULL;
    } else {
        SdbLogWarning("The context passed to cleanup function was NULL");
        return -SDBE_PTR_WAS_NULL;
    }

    return 0;
}

void *
MbPgTestServer(void *Arg)
{
}

static tg_task MbPgTasks[] = { PgThread, MbThread };

tg_group *
MbPgCreateTg(cJSON *Conf, tg_manager *Manager, i32 GroupId, sdb_arena *A)
{
    void *Ctx = MbPgInit(Conf);
    if(Ctx == NULL) {
        return NULL;
    }
    tg_group *Group = TgCreateGroup(Manager, GroupId, 2, Ctx, NULL, MbPgTasks, MbPgCleanup, A);
    return Group;
}
