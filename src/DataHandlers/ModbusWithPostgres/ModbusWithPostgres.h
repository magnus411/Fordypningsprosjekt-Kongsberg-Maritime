/**
 * @file ModbusWithPostgres.h
 * @brief Integration of Modbus and PostgreSQL data handling
 * @details Provides functionality for coordinating Modbus data collection
 * with PostgreSQL storage, including thread management and data pipe handling.
 */

#ifndef MB_W_PG_COUPLING_H
#define MB_W_PG_COUPLING_H

#include <src/Sdb.h>

#include <src/Common/SensorDataPipe.h>
#include <src/Common/ThreadGroup.h>

#include <src/Libs/cJSON/cJSON.h>

/**
 * @struct mbpg_ctx
 * @brief Context for Modbus-PostgreSQL integration
 */
typedef struct
{
    u64 ModbusMemSize;
    u64 ModbusScratchSize;
    u64 PgMemSize;
    u64 PgScratchSize;

    sensor_data_pipe *SdPipe;
    sdb_barrier       Barrier;

} mbpg_ctx;

/**
 * @brief PostgreSQL thread function
 *
 * @param Arg Pointer to mbpg_ctx structure
 * @return Thread return value (always NULL)
 */
void *PgThread(void *Arg);

/**
 * @brief Modbus thread function
 *
 * @param Arg Pointer to mbpg_ctx structure
 * @return Thread return value (always NULL)
 */

void *MbThread(void *Arg);

/**
 * @brief Data pipe throughput test thread
 *
 * @param Arg Pointer to mbpg_ctx structure
 * @return Thread return value (always NULL)
 */
void *MbPgPipeThroughputTest(void *Arg);

/**
 * @brief Modbus test server thread
 *
 * @param Arg Pointer to mbpg_ctx structure
 * @return Thread return value (always NULL)
 */
void *MbPgTestServer(void *Arg);

/**
 * @brief Cleanup function for Modbus-PostgreSQL context
 *
 * @param Arg Pointer to mbpg_ctx structure
 * @return sdb_errno 0 on success, error code on failure
 */
sdb_errno MbPgCleanup(void *Arg);

/**
 * @brief Creates thread group for Modbus-PostgreSQL integration
 *
 * @param Conf JSON configuration
 * @param GroupId Thread group identifier
 * @param A Memory arena for allocations
 * @return Initialized thread group or NULL on failure
 */
tg_group *MbPgCreateTg(cJSON *Conf, u64 GroupId, sdb_arena *A);

#endif
