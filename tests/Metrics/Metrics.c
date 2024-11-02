#if 0
#include <src/Common/Metrics.h>
#include <src/Common/SensorDataPipe.h>
#include <src/DatabaseSystems/DatabaseSystems.h>

#define SD_PIPE_BUF_COUNT 4

circular_buffer CB;


void *
tempInsert(void *arg)
{
    size_t data_size = 256;
    char  *data      = malloc(data_size);
    memset(data, 0, data_size);

    while(1) {
        CbInsert(&CB, data, data_size);
    }

    free(data);
}


void *
tempPop(void *arg)
{
    size_t data_size = 256;
    char  *data      = malloc(data_size);

    while(1) {
        CbRead(&CB, data, data_size);
    }

    free(data);
}


int
MetricTest(int argc, char const *argv[])
{


    MetricInit(&OccupancyMetric, METRIC_OCCUPANCY, "occupancy_metrics.sdb");


    MetricInit(&InputThroughput, METRIC_INPUT_THROUGHPUT, "input_throughput_metrics.sdb");
    MetricInit(&BufferWriteThroughput, METRIC_BUFFER_WRITE_THROUGHPUT,
               "buffer_write_throughput_metrics.sdb");
    MetricInit(&BufferReadThroughput, METRIC_BUFFER_READ_THROUGHPUT,
               "buffer_read_throughput_metrics.sdb");
    MetricInit(&OutputThroughput, METRIC_OUTPUT_THROUGHPUT, "output_throughput_metrics.sdb");


    sdb_arena SdbArena;
    u64       SdbArenaSize = SdbMebiByte(32);
    u8       *SdbArenaMem  = malloc(SdbArenaSize);

    SdbArenaInit(&SdbArena, SdbArenaMem, SdbArenaSize);
    CbInit(&CB, SdbMebiByte(10), &SdbArena);


    pthread_t temp1;
    pthread_t temp2;

    pthread_create(&temp1, NULL, tempInsert, NULL);
    pthread_create(&temp2, NULL, tempPop, NULL);


    pthread_join(temp1, NULL);
    pthread_join(temp2, NULL);


    MetricDestroy(&OccupancyMetric);
    MetricDestroy(&InputThroughput);
    MetricDestroy(&BufferWriteThroughput);
    MetricDestroy(&BufferReadThroughput);
    MetricDestroy(&OutputThroughput);


    return 0;
}
#endif
