/**
 * @file ModbusAPI.h
 * @brief Modbus communication API for sensor data handling
 * @details Provides interfaces for Modbus protocol operations including
 * throughput testing and main communication loop functionality.
 */

#ifndef MODBUS_API_H
#define MODBUS_API_H

#include <src/Sdb.h>


/**
 * @brief Runs a throughput test on the Modbus data pipe
 *
 * Tests the throughput capacity of the Modbus data pipe by loading and
 * processing test data. Waits at a barrier for synchronization with other
 * threads before starting the test.
 *
 * @param Arg Pointer to mbpg_ctx structure
 * @return sdb_errno 0 on success, error code on failure
 */
sdb_errno MbPipeThroughputTest(void *Arg);

/**
 * @brief Main Modbus thread loop
 *
 * Implements the main Modbus thread functionality:
 * - Initializes Modbus context and connections
 * - Manages connection lifecycle
 * - Handles data reception and parsing
 * - Implements reconnection logic
 * - pushes data to the sensor data pipe
 * - Manages graceful shutdown
 *
 * @param Arg Pointer to mbpg_ctx structure
 * @return sdb_errno 0 on success, error code on failure
 */
sdb_errno MbRun(void *Arg);

#endif
