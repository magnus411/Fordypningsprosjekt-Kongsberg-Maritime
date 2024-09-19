#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <pthread.h>

typedef struct
{
    int   protocol;
    int   sensor_id;
    void *data;
} QueueItem;

typedef struct
{
    QueueItem      *buffer;
    int             head;
    int             tail;
    int             max;
    int             full;
    pthread_mutex_t write_lock;
    pthread_mutex_t read_lock;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
} CircularBuffer;

void       InitCircularBuffer(CircularBuffer *cb, int size);
int        IsFull(CircularBuffer *cb);
int        IsEmpty(CircularBuffer *cb);
void       InsertToBuffer(CircularBuffer *cb, QueueItem *data);
QueueItem *ReadFromBuffer(CircularBuffer *cb);
void       FreeCircularBuffer(CircularBuffer *cb);

#endif /* CIRCULAR_BUFFER_H */
