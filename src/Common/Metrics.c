/**
 * @file Metrics.c
 * @brief Implementation of Metrics Tracking
 *
 * Provides concrete implementation of thread-safe  metrics collection and storage.
 *
 */


#include <pthread.h>
#include <src/Common/Metrics.h>
#include <src/Common/Thread.h>
#include <src/Sdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

SDB_LOG_REGISTER(Metrics);


metric OccupancyMetric;
metric InputThroughput;
metric BufferWriteThroughput;
metric BufferReadThroughput;
metric OutputThroughput;


/**
 * @brief Initialize Metrics Tracking Instance
 *
 * Configures metric structure with:
 * - Zero-initialized sample buffer
 * - File handle for persistent storage
 * - Thread synchronization primitives
 *
 * @param Metric Pointer to metric structure
 * @param Type Metric type classification
 * @param FileName Path for metrics file storage
 */

void
MetricInit(metric *Metric, metric_type Type, const char *FileName)
{
    memset(Metric->Samples, 0, sizeof(Metric->Samples));
    Metric->Head   = 0;
    Metric->Tail   = 0;
    Metric->Count  = 0;
    Metric->Type   = Type;
    Metric->WIndex = 0;


    Metric->File = fopen(FileName, "wb");
    if(!Metric->File) {
        SdbLogError("Failed to open metric file %s", FileName);
    }

    pthread_rwlock_init(&Metric->Rwlock, NULL);
    SdbMutexInit(&Metric->FileLock);
}


/**
 * @brief Add Performance Sample to Metrics
 *
 * @param Metric Pointer to metric structure
 * @param Data Numeric performance value
 * @return sdb_errno Success or error code
 */
sdb_errno
MetricAddSample(metric *Metric, int Data)
{
    struct timespec Timestamp;
    clock_gettime(CLOCK_MONOTONIC, &Timestamp);

    pthread_rwlock_wrlock(&Metric->Rwlock);
    Metric->Samples[Metric->Tail].Data      = Data;
    Metric->Samples[Metric->Tail].Timestamp = Timestamp;

    Metric->Tail = (Metric->Tail + 1) % MAX_SAMPLES;
    if(Metric->Count < MAX_SAMPLES) {
        Metric->Count++;
    } else {
        Metric->Head = (Metric->Head + 1) % MAX_SAMPLES;
    }

    if((Metric->Count % 40960) == 0) {
        MetricWriteToFile(Metric);
    }

    pthread_rwlock_unlock(&Metric->Rwlock);
    return 0;
}


/**
 * @brief Write Accumulated Samples to File
 *
 * @param Metric Pointer to metric structure
 * @return sdb_errno Success or error code
 */
sdb_errno
MetricWriteToFile(metric *Metric)
{
    SdbMutexLock(&Metric->FileLock, SDB_TIMEOUT_MAX);

    size_t Start = Metric->WIndex;
    size_t End   = Metric->Tail;

    if(Start == End) {
        SdbMutexUnlock(&Metric->FileLock);
        return 0;
    }

    if(End > Start) {
        // No wrap-around
        size_t TotalSamples = End - Start;
        size_t written
            = fwrite(&Metric->Samples[Start], sizeof(metric_sample), TotalSamples, Metric->File);
        if(written != TotalSamples) {
            SdbLogError("Failed to write all samples to file");
        }
    } else {
        size_t SamplesToEnd = MAX_SAMPLES - Start;
        size_t written
            = fwrite(&Metric->Samples[Start], sizeof(metric_sample), SamplesToEnd, Metric->File);
        if(written != SamplesToEnd) {
            SdbLogError("Failed to write all samples to file (first part)");
        }

        size_t written2 = fwrite(&Metric->Samples[0], sizeof(metric_sample), End, Metric->File);
        if(written2 != End) {
            SdbLogError("Failed to write all samples to file (second part)");
        }
    }

    Metric->WIndex = End;
    fflush(Metric->File);
    SdbMutexUnlock(&Metric->FileLock);

    return 0;
}


/**
 * @brief Cleanup Metrics Tracking Resources
 *
 *
 * @param Metric Pointer to metric structure
 * @return sdb_errno Success or error code
 */
sdb_errno
MetricDestroy(metric *Metric)
{
    if(Metric->File) {
        fclose(Metric->File);
        Metric->File = NULL;
    }

    Metric->Head  = 0;
    Metric->Tail  = 0;
    Metric->Count = 0;

    pthread_rwlock_destroy(&Metric->Rwlock);
    return 0;
}
