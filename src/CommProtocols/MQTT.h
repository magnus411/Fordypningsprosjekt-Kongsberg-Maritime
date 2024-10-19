#ifndef MQTT_H
#define MQTT_H

#include <MQTTClient.h>

#include <src/Sdb.h>

#include <src/CommProtocols/CommProtocols.h>
#include <src/Common/SensorDataPipe.h>

// TODO(ingar): This doesn't seem right. Is there only ever one token? Also, this is not threadsafe
// at all.
extern volatile MQTTClient_deliveryToken DeliveredToken;

typedef struct
{
    char *Address;
    char *ClientName;
    char *Topic;
    int   Qos;
    long  Timeout;

    sensor_data_pipe
        *SdPipe; // NOTE(ingar): Needed because MQTT callback functions need access to the pipe

} mqtt_ctx;

sdb_errno MqttInit(comm_protocol_api *MQTT, void *OptArgs);
sdb_errno MqttRun(comm_protocol_api *MQTT);
sdb_errno MqttFinalize(comm_protocol_api *MQTT);

#endif // MQTT_H
