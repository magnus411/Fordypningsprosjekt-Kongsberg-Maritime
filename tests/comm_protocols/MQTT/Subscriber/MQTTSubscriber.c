#include <pthread.h>
#include <string.h>
#include <MQTTClient.h>

#include <SdbExtern.h>
#include <common/CircularBuffer.h>
#include <comm_protocols/MQTT.h>

static circular_buffer Cb = { 0 };

sdb_errno
MQTTSubscriber(const char *Address, const char *ClientId, const char *Topic)
{
    CbInit(&Cb, SdbMebiByte(16));

    mqtt_subscriber MqttSubscriber;
    InitSubscriber(&MqttSubscriber, "tcp://localhost:1883", "ModbusSub", "MODBUS", 1, &Cb);

    pthread_t MqttTid;
    pthread_create(&MqttTid, NULL, MQTTSubscriberThread, &MqttSubscriber);
    pthread_join(MqttTid, NULL);

    CbFree(&Cb);

    return 0;
}
