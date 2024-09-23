#include "CircularBuffer.h"
#include "modules/ModbusModule.h"
#include <pthread.h>
#include "sdb.h"

int
TestModbus(void)
{
    CircularBuffer Cb;
    InitCircularBuffer(&Cb, SdbMebiByte(16));

    pthread_t modbus_tid;
    pthread_create(&modbus_tid, NULL, ModbusThread, &Cb);

    pthread_join(modbus_tid, NULL);

    FreeCircularBuffer(&Cb);

    return 0;
}
