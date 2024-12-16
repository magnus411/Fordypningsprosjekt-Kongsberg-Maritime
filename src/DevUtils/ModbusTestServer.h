/**
 * @file ModbusTestServer.h
 * @brief Header for Modbus Test Server Implementation
 *
 * Provides interface for a Modbus TCP test server that generates
 * and sends simulated shaft power data to connected clients.
 *
 * Key Features:
 * - Creates a TCP server listening on a configurable port
 * - Generates random shaft power data
 * - Supports non-blocking socket operations
 *
 */


#ifndef MODBUS_TEST_SERVER_H
#define MODBUS_TEST_SERVER_H

#include <src/Common/Thread.h>
#include <src/Sdb.h>

/**
 * @brief Modbus TCP port for the test server
 *
 * Uses non-standard port 1312 to avoid permission issues
 * with the traditional Modbus port (502)
 */
#define MODBUS_PORT (1312) // (502)


/**
 * @brief Runs the Modbus Test Server
 *
 * Establishes a TCP server that generates and sends simulated
 * shaft power data. The server:
 * - Listens for incoming connections
 * - Creates non-blocking sockets
 * - Sends data at a controlled frequency
 * - Handles connection and shutdown gracefully
 *
 * @param Barrier Synchronization barrier to coordinate server startup
 */
void RunModbusTestServer(sdb_barrier *Barrier);

#endif
