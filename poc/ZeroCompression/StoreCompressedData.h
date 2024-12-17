#ifndef DATA_STORAGE_H
#define DATA_STORAGE_H


/**
 * @file StoreCompressedData.h
 * @brief Header for Compressed Data Storage Utilities
 *
 * Provides interfaces for storing metadata about compressed or zero-run
 * data in a PostgreSQL database. Supports two primary storage strategies:
 *
 * 1. Run-Length Encoding (RLE) Metadata Storage
 *    - Tracks start and end times of zero data runs
 *    - Stores the length of consecutive zero data segments
 *
 * 2. Full Metadata Storage
 *    - Captures individual timestamps of zero data occurrences
 *
 * Key Functions:
 * - Database table initialization
 * - Metadata insertion for zero data segments
 *
 * @note Requires libpq (PostgreSQL) connection for operations
 * @author [Your Name]
 * @date [Current Date]
 */


#include <libpq-fe.h>
#include <src/Sdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef int sdb_errno;


/**
 * @brief Initialize Run-Length Encoded (RLE) Metadata Storage Table
 *
 * Creates a PostgreSQL table to store metadata about zero data runs,
 * including start time, end time, and run length.
 *
 * Table Schema:
 * - id: Serial primary key
 * - start_time: Timestamp of run start
 * - end_time: Timestamp of run end
 * - run_length: Number of consecutive zero data points
 *
 * @param Conn Active PostgreSQL database connection
 * @return sdb_errno 0 on success, negative on failure
 */
sdb_errno InitializeRLEMetaDataStorage(PGconn *Conn);

/**
 * @brief Store Run-Length Encoded Metadata
 *
 * Inserts a record of a zero data run into the RLE metadata table
 * using string-based time representations.
 *
 * @param Start Start time as a string
 * @param End End time as a string
 * @param RunLength Number of consecutive zero data points
 * @param Conn Active PostgreSQL database connection
 * @return sdb_errno 0 on success, negative on failure
 */
sdb_errno StoreRLEMetaData(const char *Start, const char *End, size_t RunLength, PGconn *Conn);


/**
 * @brief Store Run-Length Encoded Metadata with Timespec
 *
 * Inserts a record of a zero data run into the RLE metadata table
 * using precise timespec structures.
 *
 * @param Start Start time as a timespec structure
 * @param End End time as a timespec structure
 * @param RunLength Number of consecutive zero data points
 * @param Conn Active PostgreSQL database connection
 * @return sdb_errno 0 on success, negative on failure
 */
sdb_errno StoreRLEMetaDataWithTimespecs(const struct timespec *Start, const struct timespec *End,
                                        size_t RunLength, PGconn *Conn);


/**
 * @brief Initialize Full Metadata Storage Table
 *
 * Creates a PostgreSQL table to store individual timestamps
 * of zero data occurrences.
 *
 * Table Schema:
 * - id: Serial primary key
 * - timestamp: Individual zero data point timestamp
 *
 * @param Conn Active PostgreSQL database connection
 * @return sdb_errno 0 on success, negative on failure
 */
sdb_errno InitializeFullMetaDataStorage(PGconn *Conn);


/**
 * @brief Store Individual Metadata Timestamp
 *
 * Inserts a single timestamp of a zero data point into
 * the full metadata table.
 *
 * @param TimeStamp Timestamp of the zero data point
 * @param Conn Active PostgreSQL database connection
 * @return sdb_errno 0 on success, negative on failure
 */
sdb_errno StoreMetaData(struct timespec *TimeStamp, PGconn *Conn);

#endif // DATA_STORAGE_H
