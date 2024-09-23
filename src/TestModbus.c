#include "CircularBuffer.h"
#include "modules/ModbusModule.h"
#include <pthread.h>
#include "sdb.h"

static CircularBuffer Cb = { 0 };

int
TestModbus(void)
{
    InitCircularBuffer(&Cb, SdbMebiByte(16));

    // First Modbus Server
    ModbusArgs MbArgs;
    MbArgs.PORT = 3490;
    MbArgs.Cb   = &Cb;
    strncpy(MbArgs.Ip, "127.0.0.1", 10);

    // Second Modbus Server
    ModbusArgs MbArgs2;
    MbArgs2.PORT = 3491;
    MbArgs2.Cb   = &Cb;
    strncpy(MbArgs2.Ip, "127.0.0.1", 10);

    // Connecting to first Modbus server
    pthread_t modbus_tid;
    pthread_create(&modbus_tid, NULL, ModbusThread, &MbArgs);

    // Connecting to second Modbus server
    pthread_t modbus_tid2;
    pthread_create(&modbus_tid2, NULL, ModbusThread, &MbArgs2);

    pthread_join(modbus_tid, NULL);
    pthread_join(modbus_tid2, NULL);

    FreeCircularBuffer(&Cb);

    return 0;
}
