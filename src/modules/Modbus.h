#ifndef MODBUSMODULE_H
#define MODBUSMODULE_H

#include "../CircularBuffer.h"
#include <stdint.h>

typedef struct
{
    int     Protocol;
    int     UnitId;
    uint8_t Data[MAX_DATA_LENGTH];
    size_t  DataLength;
} QueueItem;

typedef struct
{
    int              PORT;
    char             Ip[10];
    circular_buffer *Cb;

} Modbus_Args;

void *ModbusThread(void *arg);

#endif /* MODBUSMODULE_H */
