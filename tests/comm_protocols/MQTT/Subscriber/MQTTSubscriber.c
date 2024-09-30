#include <pthread.h>
#include <errno.h>
#include <string.h>

#include "../../../../src/SdbExtern.h"
#include "../../../../src/CircularBuffer.h"
#include "../../../../src/modules/MQTT.h"

static circular_buffer Cb = { 0 };

int
MQTTSubscriber(void)
{
    InitCircularBuffer(&Cb, SdbMebiByte(16));

    MQTTSubscriber MqttArgs;
    InitSubscriber(&MqttArgs, "tcp://localhost:1883", "ModbusSub", "MODBUS", 1, &Cb);

    pthread_t MqttTid;

    pthread_create(&MqttTid, NULL, MQTTSubscriberThread, &MqttArgs);

    pthread_join(MqttTid, NULL);

    FreeCircularBuffer(&Cb);

    return 0;
}
