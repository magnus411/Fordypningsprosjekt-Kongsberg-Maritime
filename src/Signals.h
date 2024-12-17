/**
 * @file Signals.h
 * @brief Signal handling interface for the Sensor Data Handler System
 * @details Provides signal handling, crash dump functionality, and graceful
 * shutdown mechanisms for the sensor data handling system. This module manages
 * system signals and ensures proper data preservation during shutdowns or crashes.
 */

#ifndef SDB_SIGNALS_H
#define SDB_SIGNALS_H

#include <signal.h>

#include <src/Sdb.h>

#include <src/Common/SensorDataPipe.h>
#include <src/Common/ThreadGroup.h>

#include <src/Libs/cJSON/cJSON.h>

/**
 * @struct signal_handler_context
 * @brief Context structure maintaining signal handler state
 */
typedef struct signal_handler_context
{
    tg_manager           *Manager;        /**< Thread group manager instance */
    char                 *MemoryDumpPath; /**< Path for memory dump files */
    volatile sig_atomic_t ShutdownFlag;   /**< Atomic flag indicating shutdown status */
    sensor_data_pipe     *Pipe;           /**< Current sensor data pipe being monitored */

} signal_handler_context;

/** @brief Global signal handler context */
extern signal_handler_context GSignalContext;

/**
 * @brief Initializes signal handlers for the application
 *
 * Sets up handlers for system signals including SIGINT, SIGTERM,
 * SIGSEGV, SIGABRT, SIGFPE, and SIGILL.
 *
 * @param manager Thread group manager to be controlled by signals
 * @return 0 on success, -1 on failure
 */
int SdbSetupSignalHandlers(tg_manager *Manager);


/**
 * @brief Checks if the system should initiate shutdown
 *
 * @return true if shutdown should be initiated
 * @return false if operation should continue
 */
bool SdbShouldShutdown(void);

/**
 * @brief Dumps sensor data pipe contents to a file
 *
 * Creates a binary dump of the pipe's current state including all
 * buffer contents and metadata.
 *
 * @param pipe Pointer to the sensor data pipe to dump
 * @param filename Path where the dump will be saved
 * @return true if successful, false if any error occurred
 */
bool SdbDumpSensorDataPipe(sensor_data_pipe *Pipe, const char *Filename);

/**
 * @brief Converts a binary dump file to CSV format
 *
 * Transforms raw sensor data dumps into human-readable CSV files containing
 * packet information with timestamps and sensor readings.
 *
 * @param dumpFileName Source binary dump file
 * @param csvFileName Destination CSV file
 */
void ConvertDumpToCSV(const char *DumpFileName, const char *CsvFileName);

#endif // SDB_SIGNALS_H
