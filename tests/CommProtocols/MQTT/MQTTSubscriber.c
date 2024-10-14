#include <MQTTClient.h>
#include <pthread.h>
#include <string.h>

#include <src/Sdb.h>

#include <src/CommProtocols/MQTT.h>
#include <src/Common/CircularBuffer.h>

static circular_buffer Cb = { 0 };

sdb_errno
MQTTSubscriber(const char *Address, const char *ClientId, const char *Topic)
{
    CbInit(&Cb, SdbMebiByte(16), NULL);

    mqtt_args MqttSubscriber;
    InitSubscriber(&MqttSubscriber, "tcp://localhost:1883", "ModbusSub", "MODBUS", 1, &Cb);

    pthread_t MqttTid;
    pthread_create(&MqttTid, NULL, MQTTStartComm, &MqttSubscriber);
    pthread_join(MqttTid, NULL);

    CbFree(&Cb);

    return 0;
}
