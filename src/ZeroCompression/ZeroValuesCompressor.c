#include <libpq-fe.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/**
 * @file ZeroValuesCompressor.c
 * @brief Implementation of Zero Value Compression Utilities
 *
 * Provides concrete implementations for detecting, storing,
 * and compressing zero-value sensor data blocks.
 *
 * Core Functionalities:
 * - Zero value detection algorithms
 * - Metadata extraction strategies
 * - Run-Length Encoding (RLE) compression
 * - PostgreSQL metadata storage
 *
 * Key Components:
 * - Single and multi-block zero value processing
 * - Flexible storage mechanisms
 * - Efficient data compression techniques
 *
 */


#include <src/Sdb.h>
SDB_LOG_REGISTER(ZeroValuesCompressor);

#include <src/ZeroCompression/StoreCompressedData.h>

static struct timespec StartOfRLE;
static struct timespec CurrentLastZeroBlockEnd;
static int             ZeroCount;

#define SUB_BLOCK_SIZE 96 // assuming 20 for datetime and 32 for eight floats
#define DATETIME_SIZE                                                                              \
    32                // assuming zero terminated datetime as string in format: yyyy-mm-dd hh:mm:ss
#define FLOAT_COUNT 8 // assumed amount of data points for every block
struct timespec ZEROTIMESTAMP = { 0, 0 };

//"ID","TIME","PEAK_PEAK_PFS","PEAK_PEAK_KNM","MAIN_FREQ","SECOND_FREQ","TORQUE_HIGH","TORQUE_LOW","RPM_HIGH","RPM_LOW"
typedef struct __attribute__((packed))
{
    u8              Id;
    struct timespec Timestamp;
    double          PPP, PPK;
    double          MainFreq, SecondFreq;
    double          TorqueHigh, TorqueLow;
    double          RpmHigh, RpmLow;
} torsional_packet;


/**
 * @brief Check if All Values in a Single Block are Zero
 *
 * Examines a torsional packet to determine if all numeric
 * fields contain zero values.
 *
 * Checks:
 * - Peak-to-Peak PFS
 * - Peak-to-Peak KNM
 * - Main Frequency
 * - Secondary Frequency
 * - High and Low Torque
 * - High and Low RPM
 *
 * @param SingleBlock Pointer to the data block
 * @return bool True if all values are zero, false otherwise
 */
bool
CheckIfAllZeroOnSingleBlock(const unsigned char *SingleBlock)
{
    torsional_packet *Packet = (torsional_packet *)(SingleBlock);

    return (Packet->PPP == 0.0 && Packet->PPK == 0.0 && Packet->MainFreq == 0.0
            && Packet->SecondFreq == 0.0 && Packet->TorqueHigh == 0.0 && Packet->TorqueLow == 0.0
            && Packet->RpmHigh == 0.0 && Packet->RpmLow == 0.0);
}

// TODO: refactor to use CheckIfAllZeroOnSingleBlock.

/**
 * @brief Extract Metadata for Zero Data Blocks
 *
 * Scans a data block and collects timestamp information
 * for blocks containing zero values.
 *
 * Processes the block in sub-block increments, checking
 * for zero-value conditions and storing relevant metadata.
 *
 * @param DataBlock Pointer to the input data block
 * @param BlockSize Total size of the data block
 * @param Result Output array to store zero data timestamps
 * @param ResultCount Pointer to store the number of zero blocks found
 */
void
GetMetaDataWhenZeroFromBlock(const unsigned char *DataBlock, size_t BlockSize, char **Result,
                             size_t *ResultCount)
{
    *ResultCount      = 0;
    size_t BlockCount = BlockSize / SUB_BLOCK_SIZE;

    for(size_t i = 0; i < BlockCount; i++) {
        const unsigned char *BlockStart = DataBlock + i * SUB_BLOCK_SIZE;

        char BlockDateTime[DATETIME_SIZE];
        memcpy(BlockDateTime, BlockStart, DATETIME_SIZE - 1);
        BlockDateTime[DATETIME_SIZE - 1] = '\0';

        const double *Doubles = (const double *)(BlockStart + DATETIME_SIZE);

        int AllZero = 1;

        for(size_t j = 0; j < FLOAT_COUNT; j++) {
            if(Doubles[j] != 0.0f) {
                AllZero = 0;
                // TODO: Assuming we have some way of putting data into the db, here is the place to
                // do it for this method. If swapping to CheckIfAllZeroOnSingleBlock, add inserting
                // into db there instead.
                break;
            }
        }
        if(AllZero) {
            Result[*ResultCount] = (char *)malloc(DATETIME_SIZE);
            strcpy(Result[*ResultCount], BlockDateTime);
            (*ResultCount)++;
        }
    }
}

/**
 * @brief Store and Remove Zero Metadata
 *
 * Processes a data block by:
 * 1. Identifying blocks with zero values
 * 2. Storing metadata for zero blocks
 * 3. Filtering out zero blocks from the original data
 *
 * Supports flexible memory allocation through optional arena.
 *
 * @param DataBlock Input data block
 * @param BlockSize Total size of the data block
 * @param FilteredSize Pointer to store the size of non-zero data
 * @param Arena Optional memory arena for allocation
 * @param Conn PostgreSQL database connection
 * @return const unsigned char* Filtered block containing only non-zero data
 */
const unsigned char *
StoreAndRemoveAllMetaData(const unsigned char *DataBlock, size_t BlockSize, size_t *FilteredSize,
                          sdb_arena *Arena, PGconn *Conn)
{
    size_t BlockCount = BlockSize / SUB_BLOCK_SIZE;
    size_t ValidCount = 0;

    for(size_t i = 0; i < BlockCount; i++) {
        const unsigned char *BlockStart = DataBlock + i * SUB_BLOCK_SIZE;

        if(!CheckIfAllZeroOnSingleBlock(BlockStart)) {
            ++ValidCount;
        } else {
            torsional_packet *Packet = (torsional_packet *)BlockStart;
            StoreMetaData((struct timespec *)(Packet + offsetof(torsional_packet, Timestamp)),
                          Conn);
        }
    }

    size_t         AllocSize = ValidCount * SUB_BLOCK_SIZE;
    unsigned char *FilteredBlock;

    if(NULL == Arena) {
        FilteredBlock = malloc(AllocSize);
    } else {
        FilteredBlock = SdbArenaPush(Arena, AllocSize);
    }

    if(NULL == FilteredBlock) {
        SdbLogError("Error when allocating memory. Occurred when allocating memory for "
                    "StoreAndRemoveAllMetaData");
        return NULL;
    }

    unsigned char *FilteredPtr = FilteredBlock;
    for(size_t i = 0; i < BlockCount; i++) {
        const unsigned char *BlockStart = DataBlock + i * SUB_BLOCK_SIZE;

        if(!CheckIfAllZeroOnSingleBlock(BlockStart)) {
            memcpy(FilteredPtr, BlockStart, SUB_BLOCK_SIZE);
            FilteredPtr += SUB_BLOCK_SIZE;
        }
    }

    *FilteredSize = ValidCount * SUB_BLOCK_SIZE;

    return FilteredBlock;
}

/**
 * @brief Save Metadata for Zero Values in a Single Block
 *
 * Checks a single block for zero values and stores
 * its timestamp metadata in the database.
 *
 * @param SingleBlock Pointer to the data block to check
 * @param Conn PostgreSQL database connection
 */
void
SingleBlockSaveMetaDataWhenZero(const unsigned char *SingleBlock, PGconn *Conn)
{
    bool IsZero = CheckIfAllZeroOnSingleBlock(SingleBlock);

    if(IsZero) {
        StoreMetaData((struct timespec *)(SingleBlock + offsetof(torsional_packet, Timestamp)),
                      Conn);
    }
}

// Saves every instance of metadata where data is zero for multi blocks.
void
MultiBlockSaveAllMetaDataWhenZero(const unsigned char *MultiBlock, size_t BlockSize)
{
    size_t BlockCount = BlockSize / SUB_BLOCK_SIZE;

    for(size_t i = 0; i < BlockCount; i++) {
        const unsigned char *BlockStart = MultiBlock + i * SUB_BLOCK_SIZE;

        char BlockDateTime[DATETIME_SIZE];
        memcpy(BlockDateTime, BlockStart, DATETIME_SIZE - 1);
        BlockDateTime[DATETIME_SIZE - 1] = '\0';

        const double *Doubles = (const double *)(BlockStart + DATETIME_SIZE);

        for(size_t j = 0; j < FLOAT_COUNT; j++) {
            if(Doubles[j] == 0.0f) {
                // Store data metadata from BlockStart and for size DATETIME_SIZE.
            }
        }
    }
}

/**
 * @brief Mapping of Block Zero/Non-Zero Status
 *
 * Creates a binary map indicating the zero/non-zero status
 * of each block in a multi-block dataset.
 *
 * @param MultiBlock Pointer to the multi-block data
 * @param BlockSize Total size of the multi-block data
 * @param Map Output array to store block status mapping
 * @return sdb_errno 0 on success, negative on failure
 */
sdb_errno
MapMultiBlock(const unsigned char *MultiBlock, size_t BlockSize, int *Map)
{
    size_t BlockCount = BlockSize / SUB_BLOCK_SIZE;

    for(size_t i = 0; i < BlockCount; ++i) {
        const unsigned char *Block  = MultiBlock + i * SUB_BLOCK_SIZE;
        bool                 IsZero = CheckIfAllZeroOnSingleBlock(Block);
        if(IsZero) {
            Map[i] = 0;
            return 0;
        } else {
            Map[i] = 1;
            return 0;
        }
    }
    return -1;
}


/**
 * @brief Full Run-Length Encoded Zero Compression
 *
 * Implements a comprehensive zero data compression strategy
 * using Run-Length Encoding (RLE) across multiple blocks.
 *
 * Tracks:
 * - Start of zero data streak
 * - End of zero data streak
 * - Number of consecutive zero blocks
 *
 * Stores RLE metadata in PostgreSQL when a zero streak ends.
 *
 * @param MultiBlock Pointer to the multi-block data
 * @param BlockSize Total size of the multi-block data
 * @param Conn PostgreSQL database connection
 * @return sdb_errno 0 on success, negative on failure
 */
sdb_errno
FullRLEZeroCompression(const unsigned char *MultiBlock, size_t BlockSize, PGconn *Conn)
{
    size_t BlockCount = BlockSize / SUB_BLOCK_SIZE;
    for(size_t i = 0; i < BlockCount; ++i) {
        const unsigned char *Block  = MultiBlock + i * SUB_BLOCK_SIZE;
        bool                 IsZero = CheckIfAllZeroOnSingleBlock(Block);
        if(IsZero) {
            torsional_packet *Packet = (torsional_packet *)(Block);
            if(StartOfRLE.tv_sec == 0 && StartOfRLE.tv_nsec == 0) {
                StartOfRLE = Packet->Timestamp;
            }
            CurrentLastZeroBlockEnd = Packet->Timestamp;
            ZeroCount++;
            return 0;
        } else {
            StoreRLEMetaDataWithTimespecs(&StartOfRLE, &CurrentLastZeroBlockEnd, ZeroCount, Conn);
            ZeroCount               = 0;
            StartOfRLE              = ZEROTIMESTAMP;
            CurrentLastZeroBlockEnd = ZEROTIMESTAMP;
            return 0;
        }
    }
    return 0;
}
