#include <pthread.h>
#include <src/Common/Thread.h>
#include <src/Metrics.h>
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


void
InitMetric(metric *Metric, metric_type Type, const char *FileName)
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

sdb_errno
AddSample(metric *Metric, int Data)
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
        WriteMetricToFile(Metric);
    }

    pthread_rwlock_unlock(&Metric->Rwlock);
    return 0;
}

sdb_errno
WriteMetricToFile(metric *Metric)
{
    SdbMutexLock(&Metric->FileLock, SDB_TIMEOUT_MAX);

    size_t start = Metric->WIndex;
    size_t end   = Metric->Tail;

    if(start == end) {
        SdbMutexUnlock(&Metric->FileLock);
        return 0;
    }

    if(end > start) {
        // No wrap-around
        size_t total_samples = end - start;
        size_t written
            = fwrite(&Metric->Samples[start], sizeof(sample), total_samples, Metric->File);
        if(written != total_samples) {
            SdbLogError("Failed to write all samples to file");
        }
    } else {
        size_t samples_to_end = MAX_SAMPLES - start;
        size_t written
            = fwrite(&Metric->Samples[start], sizeof(sample), samples_to_end, Metric->File);
        if(written != samples_to_end) {
            SdbLogError("Failed to write all samples to file (first part)");
        }

        size_t written2 = fwrite(&Metric->Samples[0], sizeof(sample), end, Metric->File);
        if(written2 != end) {
            SdbLogError("Failed to write all samples to file (second part)");
        }
    }

    Metric->WIndex = end;

    fflush(Metric->File);

    printf("Printed\n");

    SdbMutexUnlock(&Metric->FileLock);

    return 0;
}


sdb_errno
DestroyMetric(metric *Metric)
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
