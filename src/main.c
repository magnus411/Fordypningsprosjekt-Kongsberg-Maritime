#include "CircularBuffer.h"
#include "ModbusModule.h"
#include <pthread.h>

int
main()
{
    CircularBuffer Cb;
    InitCircularBuffer(&Cb, 1000);

    pthread_t modbus_tid;
    pthread_create(&modbus_tid, NULL, ModbusThread, &Cb);

    pthread_join(modbus_tid, NULL);

    FreeCircularBuffer(&Cb);

    return 0;
}
