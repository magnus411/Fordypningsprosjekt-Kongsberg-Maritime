/**
 * @file PostgresAPI.h
 * @brief PostgreSQL database thread interface
 * @details Provides the interface for PostgreSQL database operations in a 
 * multi-threaded environment, handling sensor data storage and management.
 */


#ifndef POSTGRES_API_H
#define POSTGRES_API_H

#include <src/Sdb.h>

/**
 * @brief Main PostgreSQL thread function
 *
 * Manages the PostgreSQL database operations:
 * - Initializes database connection
 * - Processes sensor data from pipe
 * - Handles data insertion with timing metrics
 * - Manages graceful shutdown
 * 
 * @param Arg Pointer to mbpg_ctx structure
 * @return sdb_errno 0 on success, error code on failure:
 *         - -ETIMEDOUT: Connection timeout
 *         - -EIO: I/O error
 *         - -errno: System error codes
 */
sdb_errno PgRun(void *Arg);

#endif
