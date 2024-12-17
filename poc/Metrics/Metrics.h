#ifndef METRICS_H
#define METRICS_H

/**
 * @file Metrics.h
 * @brief High-Performance Metrics Tracking System
 *
 * API for collecting metrics across the system.
 *
 */

#include <pthread.h>
#include <src/Common/Thread.h>
#include <src/Sdb.h>
#include <stdlib.h>
#include <time.h>

/** @brief Maximum number of samples per metric */
#define MAX_SAMPLES 4194304

/**
 * @brief Enumeration of supported metric types
 *
 * Provides predefined categories for system performance tracking
 */
typedef enum
{
    METRIC_OCCUPANCY,               ///< Buffer or resource occupancy
    METRIC_INPUT_THROUGHPUT,        ///< Input data processing rate
    METRIC_BUFFER_WRITE_THROUGHPUT, ///< Buffer write performance
    METRIC_BUFFER_READ_THROUGHPUT,  ///< Buffer read performance
    METRIC_OUTPUT_THROUGHPUT        ///< Output data processing rate
} metric_type;

/**
 * @brief Represents a single performance metric sample
 *
 * Captures both the numeric data and precise timestamp
 */
typedef struct
{
    int             Data;      ///< Numeric metric value
    struct timespec Timestamp; ///< High-resolution timestamp
} metric_sample;

/**
 * @brief  Metrics Tracking Structure
 *
 * Implements a circular buffer for storing performance samples
 * with concurrent access controls and file-based persistence
 */
typedef struct
{
    metric_type      Type;                 ///< Metric type classification
    metric_sample    Samples[MAX_SAMPLES]; ///< Circular buffer of samples
    size_t           Head;                 ///< Start of valid data
    size_t           Tail;                 ///< End of valid data
    size_t           Count;                ///< Current number of samples
    pthread_rwlock_t Rwlock;               ///< Read-write lock for thread safety
    FILE            *File;                 ///< File for persistent storage
    sdb_mutex        FileLock;             ///< Mutex for file writing
    size_t           WIndex;               ///< Current write index
} metric;

/**
 * @brief Initialize a Metrics Tracking Instance
 *
 * Sets up the metric structure, opens a file for persistence
 *
 * @param Metric Pointer to metric structure
 * @param Type Metric type classification
 * @param FileName Path for metrics file storage
 */
void MetricInit(metric *Metric, metric_type Type, const char *FileName);

/**
 * @brief Add a Sample to Metrics Tracking
 *
 * Method to record a performance sample
 *
 * @param Metric Pointer to metric structure
 * @param Data Numeric value to record
 * @return sdb_errno Success or error code
 */
sdb_errno MetricAddSample(metric *Metric, int Data);

/**
 * @brief Write Accumulated Samples to Persistent Storage
 *
 * Flushes samples to file, managing thread-safe file writing
 *
 * @param Metric Pointer to metric structure
 * @return sdb_errno Success or error code
 */
sdb_errno MetricWriteToFile(metric *Metric);

/**
 * @brief Clean Up Metrics Tracking Resources
 *
 * Closes file handles and resets metric structure
 *
 * @param Metric Pointer to metric structure
 * @return sdb_errno Success or error code
 */
sdb_errno MetricDestroy(metric *Metric);

// Global Metric Instances
extern metric OccupancyMetric;
extern metric InputThroughput;
extern metric BufferWriteThroughput;
extern metric BufferReadThroughput;
extern metric OutputThroughput;

#endif