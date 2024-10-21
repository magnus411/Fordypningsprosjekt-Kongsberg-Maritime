#include <pthread.h>
#include <src/Common/Thread.h>
#include <src/Metrics.h>
#include <src/Sdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
SDB_LOG_REGISTER(Metrics);

static metric System;


static metric receiveTimeMetric;
static metric insertTimeMetric;
static metric framesProcessedMetric;

void
InitMetricSystem(metric_type Type)
{

    memset(System.Samples, 0, sizeof(System.Samples));
    System.Head  = 0;
    System.Tail  = 0;
    System.Count = 0;
    System.Type  = Type;


    System.File = fopen("metrics.sdb", "w");

    pthread_rwlock_init(&System.Rwlock, NULL);
    SdbMutexInit(&System.FileLock);
}
// TODO! return sdb_errno

sdb_errno
WriteMetricToFile()
{

    SdbMutexLock(&System.FileLock, SDB_TIMEOUT_MAX);

    size_t w = fwrite(System.Samples, sizeof(*System.Samples), MAX_SAMPLES, System.File);

    SdbMutexUnlock(&System.FileLock);
}


sdb_errno
AddSample(int Data)
{

    struct timespec Timestamp;
    clock_gettime(CLOCK_MONOTONIC, &Timestamp);

    pthread_rwlock_wrlock(&System.Rwlock);
    System.Samples[System.Tail].Data      = Data;
    System.Samples[System.Tail].Timestamp = Timestamp;

    System.Tail = (System.Tail + 1) % MAX_SAMPLES;
    if(System.Count < MAX_SAMPLES) {
        System.Count++;
    } else {
        System.Head = (System.Head + 1) % MAX_SAMPLES;
    }

    if((System.Count % 50) == 50) {
        WriteMetricToFile();
    }

    pthread_rwlock_unlock(&System.Rwlock);
    return 0;
}

// TODO! return sdb_errno

sdb_errno
DestroyMetricSystem(void)
{

    free(System.Samples);

    System.Head  = 0;
    System.Tail  = 0;
    System.Count = 0;

    pthread_rwlock_destroy(&System.Rwlock);
    return 0;
}


void *
tempOcc(void *)
{
    while(1) {


        int             num       = System.Samples[System.Tail - 1].Data;
        struct timespec timestamp = System.Samples[System.Tail - 1].Timestamp;

        int count = System.Count;
        printf("%d  Filled \n", num);
        printf("%ld seconds,\n", timestamp.tv_sec);
        printf("%d\n", count);

        usleep(1000000);
    }
}