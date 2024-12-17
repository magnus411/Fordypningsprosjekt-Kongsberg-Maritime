#ifndef TEST_CONSTANTS_H
#define TEST_CONSTANTS_H

/**
 * @file TestConstants.h
 * @brief Constants for Modbus Test Server
 */
#include <src/Sdb.h>

#include <src/DatabaseSystems/Postgres.h>

typedef struct __attribute__((packed, aligned(1)))
{
    pg_int8 PacketId;
    time_t  Time; // NOTE(ingar): The insert function assumes a time_t comes in and converts it to a
                  // pg_timestamp
    pg_float8 Rpm;
    pg_float8 Torque;
    pg_float8 Power;
    pg_float8 PeakPeakPfs;

} shaft_power_data;

#endif
