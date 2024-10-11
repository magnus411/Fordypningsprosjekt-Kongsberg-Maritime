#include <stdint.h>

#include <SdbExtern.h>
#include <common/CircularBuffer.h>

#ifndef MODBUSMODULE_H
#define MODBUSMODULE_H

typedef struct
{
    int              PORT;
    char             Ip[10];
    circular_buffer *Cb;

} modbus_args;

void *ModbusThread(void *arg);

#endif /* MODBUSMODULE_H */
