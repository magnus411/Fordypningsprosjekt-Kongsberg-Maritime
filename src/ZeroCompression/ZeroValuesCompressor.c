#include <libpq-fe.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

bool
CheckIfAllZeroOnSingleBlock(const unsigned char *SingleBlock)
{
    torsional_packet *Packet = (torsional_packet *)(SingleBlock);

    return (Packet->PPP == 0.0 && Packet->PPK == 0.0 && Packet->MainFreq == 0.0
            && Packet->SecondFreq == 0.0 && Packet->TorqueHigh == 0.0 && Packet->TorqueLow == 0.0
            && Packet->RpmHigh == 0.0 && Packet->RpmLow == 0.0);
}

// TODO: refactor to use CheckIfAllZeroOnSingleBlock.
// Make this return the non-zero data for storage outside of this method
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

// Takes a given amount of datablocks and stores the ones thats zero in its respective table.
// returns the blocks that were not zero.
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

// Saves every instance of zero datas metadata, handling single blocks.
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

/* Thoughts: In order to handle complexity of blocks that are not all zeroes or all valid, is to map
 * every sub block to 1's or 0's and return the map. The outer program will then handle storage of
 * the non-zero data.
 *
 * Using the mapping will make it easy to store every zero using RLE even if length is one. */
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
