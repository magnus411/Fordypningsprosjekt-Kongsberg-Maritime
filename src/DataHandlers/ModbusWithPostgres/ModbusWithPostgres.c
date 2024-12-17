/**
 * @file ModbusWithPostgres.c
 * @brief Implementation of Modbus-PostgreSQL integration
 */

#define _GNU_SOURCE

#include <src/Sdb.h>
SDB_LOG_REGISTER(MbWithPg);

#include <src/Common/SensorDataPipe.h>
#include <src/Common/ThreadGroup.h>
#include <src/DataHandlers/DataHandlers.h>
#include <src/DataHandlers/ModbusWithPostgres/Modbus.h>
#include <src/DataHandlers/ModbusWithPostgres/ModbusWithPostgres.h>
#include <src/DataHandlers/ModbusWithPostgres/Postgres.h>
#include <src/DevUtils/ModbusTestServer.h>

#include <src/Libs/cJSON/cJSON.h>

#include <pthread.h>
#include <src/Signals.h>


/**
 * @brief PostgreSQL thread implementation
 *
 * Sets thread name and runs PostgreSQL operations. Handles errors
 * and performs graceful shutdown.
 *
 * @param Arg Pointer to mbpg_ctx structure
 * @return NULL
 */
void *
PgThread(void *Arg)
{

    pthread_setname_np(pthread_self(), "postgres-thread");

    sdb_errno Ret = PgRun(Arg);
    if(Ret != 0) {
        SdbLogError("Postgres thread exited with error code %d (%s)", Ret, SdbStrErr(Ret));
    }

    SdbLogInfo("Postgres thread shutting down");
    return NULL;
}


/**
 * @brief Modbus thread implementation
 *
 * Sets thread name and runs Modbus operations. Handles errors
 * and performs graceful shutdown.
 *
 * @param Arg Pointer to mbpg_ctx structure
 * @return NULL
 */
void *
MbThread(void *Arg)
{
    pthread_setname_np(pthread_self(), "modbus-thread");

    sdb_errno Ret = MbRun(Arg);
    if(Ret != 0) {
        SdbLogError("Modbus thread exited with error code %d (%s)", Ret, SdbStrErr(Ret));
    }

    SdbLogInfo("Modbus thread shutting down");
    return NULL;
}


/**
 * @brief Modbus test server thread
 *
 * Sets thread name and runs the Modbus test server. Handles errors
 * and performs graceful shutdown.
 *
 * @param Arg Pointer to mbpg_ctx context.
 * @return NULL
 */
void *
MbPgTestServer(void *Arg)
{
    pthread_setname_np(pthread_self(), "modbus-test-server-thread");
    mbpg_ctx *Ctx = Arg;

    RunModbusTestServer(&Ctx->Barrier);

    SdbLogInfo("Modbus test server thread shutting down");
    return NULL;
}


/**
 * @brief Modbus data pipe throughput test
 *
 * Tests the throughput capacity of the Modbus data pipe by loading and
 * processing test data. Waits at a barrier for synchronization with other
 * threads before starting the test.
 *
 * @param Arg Pointer to mbpg_ctx structure
 * @return sdb_errno 0 on success, error code on failure
 */
void *
MbPgPipeThroughputTest(void *Arg)
{
    while(!SdbShouldShutdown()) {
        sdb_errno Ret = MbPipeThroughputTest(Arg);
        if(Ret != 0) {
            SdbLogError("Modbus pipe throughput test exited with error code %d (%s)", Ret,
                        SdbStrErr(Ret));
            break;
        }
    }
    SdbLogInfo("Throughput test thread shutting down");
    return NULL;
}


/**
 * @brief Parses memory configuration from JSON
 *
 * Extracts memory and scratch sizes from configuration file.
 *
 * @param[in] Conf JSON configuration object
 * @param[out] MemSize Memory size value
 * @param[out] ScratchSize Scratch memory size value
 */
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

/** Array of task functions for thoughout test */
static tg_task MbPgThroughputTestTasks[] = { PgThread, MbPgPipeThroughputTest };


/**< Array of task functions. Starts postgres thread, modbus test server thread and modbus thread */
static tg_task MbPgTestTasks[] = {
    PgThread,
    MbPgTestServer,
    MbThread,
};


/**
 * @brief Creates thread group from configuration
 *
 * Creates and initializes a thread group based on JSON configuration:
 * 1. Parses Modbus and PostgreSQL configurations
 * 2. Allocates and initializes context
 * 3. Sets up data pipe
 * 4. Configures thread tasks based on mode (test/normal)
 *
 * @param Conf JSON configuration
 * @param GroupId Thread group identifier
 * @param A Memory arena for allocations
 * @return Initialized thread group or NULL on failure
 */
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
#if 1
        SdbLogError("Throughput test is currently not functional. Please set \"enabled\" in "
                    "\"testing\" to true and run the program again");
        return NULL;
#else
        Group = TgCreateGroup(GroupId, 2, Ctx, NULL, MbPgThroughputTestTasks, MbPgCleanup, A);
        SdbBarrierInit(&Ctx->Barrier, 2);
#endif
    }

    GSignalContext.Pipe = Ctx->SdPipe;

    return Group;
}
