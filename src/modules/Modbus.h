#include <stdint.h>

#include "../CircularBuffer.h"
#include "../SdbExtern.h"

#ifndef MODBUSMODULE_H
#define MODBUSMODULE_H

typedef struct
{
    int    Protocol;
    int    UnitId;
    u8     Data[MAX_DATA_LENGTH];
    size_t DataLength;
} QueueItem;

typedef struct
{
    int              PORT;
    char             Ip[10];
    circular_buffer *Cb;

} Modbus_Args;

void *ModbusThread(void *arg);

#endif /* MODBUSMODULE_H */
