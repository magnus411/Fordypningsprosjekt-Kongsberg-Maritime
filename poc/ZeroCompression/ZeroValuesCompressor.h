#ifndef SENSOR_DATA_UTILS_H
#define SENSOR_DATA_UTILS_H

#include "src/StoreCompressedData.h"
#include <libpq-fe.h>
#include <src/Sdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/**
 * @file ZeroValuesCompressor.h
 * @brief Header for Zero Value Compression Utilities
 *
 * Provides interfaces for detecting, storing, and compressing 
 * zero-value sensor data blocks. Supports various metadata 
 * storage strategies for zero data segments.
 *
 * Key Functionalities:
 * - Zero value detection in data blocks
 * - Metadata extraction for zero data
 * - Run-Length Encoding (RLE) of zero data
 * - Flexible storage mechanisms
 *
 */


#define SUB_BLOCK_SIZE 84
#define DATETIME_SIZE  20
#define FLOAT_COUNT    8


/**
 * @brief Check if an entire block contains zero values
 * 
 * Determines whether all data points in a single block 
 * are zero across multiple sensor measurements.
 * 
 * @param SingleBlock Pointer to the data block to check
 * @return bool True if all values are zero, false otherwise
 */
bool CheckIfAllZeroOnSingleBlock(const unsigned char *SingleBlock);

/**
 * @brief Store Run-Length Encoded Zero Data Streak
 * 
 * Stores metadata for a consecutive sequence of zero data 
 * points, identified by their timestamps.
 * 
 * @param DateTimes Array of datetime strings
 * @param Count Number of zero data points in the streak
 */
void StoreRLEZeroStreak(char DateTimes[][DATETIME_SIZE], size_t Count);


/**
 * @brief Extract Metadata for Zero Data Blocks
 * 
 * Identifies and extracts timestamp information for 
 * blocks containing only zero values.
 * 
 * @param DataBlock Pointer to the data block
 * @param BlockSize Total size of the data block
 * @param Result Output array to store zero data timestamps
 * @param ResultCount Pointer to store the number of zero data blocks found
 */
void GetMetaDataWhenZeroFromBlock(const unsigned char *DataBlock, size_t BlockSize, char **Result,
                                  size_t *ResultCount);


                                  /**
 * @brief Save Metadata for Zero Values in a Single Block
 * 
 * Checks a single block for zero values and stores 
 * its metadata if applicable.
 * 
 * @param SingleBlock Pointer to the data block to check
 */
void SingleBlockSaveMetaDataWhenZero(const unsigned char *SingleBlock);

/**
 * @brief Save Metadata for Zero Values Across Multiple Blocks
 * 
 * Scans multiple data blocks and stores metadata for 
 * any zero value occurrences.
 * 
 * @param MultiBlock Pointer to the multi-block data
 * @param BlockSize Total size of the multi-block data
 */
void MultiBlockSaveAllMetaDataWhenZero(const unsigned char *MultiBlock, size_t BlockSize);


/**
 * @brief Create a Mapping of Zero and Non-Zero Blocks
 * 
 * Generates a binary map indicating which blocks contain 
 * zero values (0) and which contain non-zero values (1).
 * 
 * @param MultiBlock Pointer to the multi-block data
 * @param BlockSize Total size of the multi-block data
 * @param Map Output array to store the block status mapping
 * @return sdb_errno 0 on success, negative on failure
 */
sdb_errno MapMultiBlock(const unsigned char *MultiBlock, size_t BlockSize, int *Map);

/**
 * @brief Perform Full Run-Length Encoding Zero Compression
 * 
 * Implements a comprehensive zero data compression strategy 
 * using Run-Length Encoding (RLE) across multiple blocks.
 * 
 * @param MultiBlock Pointer to the multi-block data
 * @param BlockSize Total size of the multi-block data
 * @param Conn PostgreSQL database connection
 * @return sdb_errno 0 on success, negative on failure
 */
sdb_errno FunnRLEZeroCompression(const unsigned char *MultiBlock, size_t BlockSize, PGconn *Conn);

#endif // SENSOR_DATA_UTILS_H
