#include "CircularBuffer.h"
#include <stdio.h>
#include <stdlib.h>

void
InitCircularBuffer(CircularBuffer *cb, int size)
{
    cb->buffer = (QueueItem *)malloc(sizeof(QueueItem) * size);
    cb->max    = size;
    cb->head   = 0;
    cb->tail   = 0;
    cb->full   = 0;

    pthread_mutex_init(&cb->write_lock, NULL);
    pthread_mutex_init(&cb->read_lock, NULL);
    pthread_cond_init(&cb->not_empty, NULL);
    pthread_cond_init(&cb->not_full, NULL);
}

int
IsFull(CircularBuffer *cb)
{
    return cb->full;
}

int
IsEmpty(CircularBuffer *cb)
{
    return (!cb->full && (cb->head == cb->tail));
}

void
InsertToBuffer(CircularBuffer *cb, QueueItem *data)
{
    pthread_mutex_lock(&cb->write_lock);

    while(IsFull(cb))
    {
        pthread_cond_wait(&cb->not_full, &cb->write_lock);
    }

    cb->buffer[cb->head] = memcopy(data, sizeof(QueueItem));
    cb->head             = (cb->head + 1) % cb->max;

    if(cb->head == cb->tail)
    {
        cb->full = 1;
    }

    pthread_cond_signal(&cb->not_empty);
    pthread_mutex_unlock(&cb->write_lock);
}

QueueItem *
ReadFromBuffer(CircularBuffer *cb)
{
    pthread_mutex_lock(&cb->read_lock);

    while(IsEmpty(cb))
    {
        pthread_cond_wait(&cb->not_empty, &cb->read_lock);
    }

    QueueItem *data = &cb->buffer[cb->tail];
    cb->full        = 0;
    cb->tail        = (cb->tail + 1) % cb->max;

    pthread_cond_signal(&cb->not_full);
    pthread_mutex_unlock(&cb->read_lock);

    return data;
}

void
FreeCircularBuffer(CircularBuffer *cb)
{
    free(cb->buffer);
    pthread_mutex_destroy(&cb->write_lock);
    pthread_mutex_destroy(&cb->read_lock);
    pthread_cond_destroy(&cb->not_empty);
    pthread_cond_destroy(&cb->not_full);
}
