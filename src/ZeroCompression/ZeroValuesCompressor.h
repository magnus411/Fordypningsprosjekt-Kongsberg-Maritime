#ifndef SENSOR_DATA_UTILS_H
#define SENSOR_DATA_UTILS_H

#include "src/StoreCompressedData.h"
#include <libpq-fe.h>
#include <src/Sdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUB_BLOCK_SIZE 84
#define DATETIME_SIZE  20
#define FLOAT_COUNT    8

bool CheckIfAllZeroOnSingleBlock(const unsigned char *SingleBlock);
void StoreRLEZeroStreak(char DateTimes[][DATETIME_SIZE], size_t Count);
void GetMetaDataWhenZeroFromBlock(const unsigned char *DataBlock, size_t BlockSize, char **Result,
                                  size_t *ResultCount);
void SingleBlockSaveMetaDataWhenZero(const unsigned char *SingleBlock);
void MultiBlockSaveAllMetaDataWhenZero(const unsigned char *MultiBlock, size_t BlockSize);
sdb_errno MapMultiBlock(const unsigned char *MultiBlock, size_t BlockSize, int *Map);
sdb_errno FunnRLEZeroCompression(const unsigned char *MultiBlock, size_t BlockSize, PGconn *Conn);

#endif // SENSOR_DATA_UTILS_H
