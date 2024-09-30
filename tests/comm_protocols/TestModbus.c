#include <pthread.h>
#include <errno.h>

#define SDB_LOG_LEVEL 4

#include "sdb.h"
#include "CircularBuffer.h"
#include "modules/Modbus.h"

SDB_LOG_REGISTER(TestModbus);

static circular_buffer Cb = { 0 };

int
TestModbus(void)
{
    InitCircularBuffer(&Cb, SdbMebiByte(16));

    // First Modbus Server
    modbus_args MbArgs;
    MbArgs.PORT = 3490;
    MbArgs.Cb   = &Cb;
    strncpy(MbArgs.Ip, "127.0.0.1", 10);

    // Connecting to first Modbus server
    pthread_t ModbusTid;
    pthread_create(&ModbusTid, NULL, ModbusThread, &MbArgs);

    pthread_join(ModbusTid, NULL);

    FreeCircularBuffer(&Cb);

    return 0;
}
