#ifndef METRICS_H
#define METRICS_H

#include <pthread.h>
#include <src/Common/Thread.h>
#include <src/Sdb.h>
#include <stdlib.h>
#include <time.h>

#define MAX_SAMPLES 4194304 

typedef enum
{
    METRIC_OCCUPANCY,
    METRIC_INPUT_THROUGHPUT,
    METRIC_BUFFER_WRITE_THROUGHPUT,
    METRIC_BUFFER_READ_THROUGHPUT,
    METRIC_OUTPUT_THROUGHPUT
} metric_type;


typedef struct
{
    int             Data;
    struct timespec Timestamp;
} sample;

typedef struct
{
    metric_type      Type;
    sample           Samples[MAX_SAMPLES];
    size_t           Head;
    size_t           Tail;
    size_t           Count;
    pthread_rwlock_t Rwlock;
    FILE            *File;
    sdb_mutex        FileLock;
    size_t           WIndex;
} metric;

void      MetricInit(metric *Metric, metric_type Type, const char *FileName);
sdb_errno MetricAddSample(metric *Metric, int Data);
sdb_errno MetricWriteToFile(metric *Metric);
sdb_errno MetricDestroy(metric *Metric);


extern metric OccupancyMetric;
extern metric InputThroughput;
extern metric BufferWriteThroughput;
extern metric BufferReadThroughput;
extern metric OutputThroughput;


#endif
