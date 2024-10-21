#ifndef METRICS_H
#define METRICS_H

#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <src/Sdb.h>
#include <src/Common/Thread.h>

#define MAX_SAMPLES 1000

typedef enum {
    METRIC_THROUGHPUT,
    METRIC_OCCUPANCY,
    METRIC_COUNT 
} metric_type;

typedef struct {
    int Data;
    struct timespec Timestamp;
} sample;

typedef struct {
    metric_type Type;
    sample Samples[MAX_SAMPLES];   
    size_t Head;                   
    size_t Tail;                   
    size_t Count;                  
    pthread_rwlock_t Rwlock; 

    FILE *File;
    sdb_mutex FileLock;      
} metric;



void InitMetricSystem(metric_type Type);
sdb_errno AddSample(int Data);
sdb_errno DestroyMetricSystem(void);
void *tempOcc(void*);
#endif
