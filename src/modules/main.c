#include "ModbusModule.h"
#include "CircularBuffer.h"
#include <pthread.h>

int
main()
{
    CircularBuffer cb;
    InitCircularBuffer(&cb, 10);

    pthread_t modbus_tid;
    pthread_create(&modbus_tid, NULL, modbus_thread, &cb);

    pthread_join(modbus_tid, NULL);

    FreeCircularBuffer(&cb);

    return 0;
}
