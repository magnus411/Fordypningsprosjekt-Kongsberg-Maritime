/**
 * @file MQTT.h
 * @brief MQTT protocol implementation interface (Experimental)
 * @warning This is experimental code currently disabled in compilation
 */

#ifndef MQTT_H
#define MQTT_H

#include <MQTTClient.h>
#include <src/Sdb.h>

SDB_BEGIN_EXTERN_C

#include <src/Common/SensorDataPipe.h>


extern volatile MQTTClient_deliveryToken DeliveredToken;

/**
 * @struct mqtt_ctx
 * @brief Context structure for MQTT operations
 */
typedef struct
{
    char *Address;    /**< MQTT broker address */
    char *ClientName; /**< Client identifier */
    char *Topic;      /**< Subscription topic */
    int   Qos;        /**< Quality of Service level */
    long  Timeout;    /**< Operation timeout in milliseconds */

    sensor_data_pipe *SdPipe; /**< Data pipe for sensor data
                                  @note Required for callback access */
} mqtt_ctx;


void ConnLost(void *Ctx, char *Cause);

/**
 * @brief Message arrival callback
 *
 * @param Ctx_ Context pointer
 * @param TopicName MQTT topic name
 * @param TopicLen Topic name length
 * @param Message MQTT message structure
 * @return 1 for success, 0 to retry
 */
int MsgArrived(void *Ctx_, char *TopicName, int TopicLen, MQTTClient_message *Message);

/**
 * @brief Initializes MQTT subscriber
 *
 * @param Ctx MQTT context
 * @param Address Broker address
 * @param ClientId Client identifier
 * @param Topic Subscription topic
 * @param Qos Quality of Service level
 * @param SdPipe Sensor data pipe
 * @param Arena Memory arena
 * @return sdb_errno Success/error code
 */
sdb_errno InitSubscriber(mqtt_ctx *Ctx, const char *Address, const char *ClientId,
                         const char *Topic, int Qos, sensor_data_pipe *SdPipe, sdb_arena *Arena);

/**
 * @brief Initializes MQTT protocol handler
 *
 * @param MQTT Protocol API structure
 * @return sdb_errno Success/error code
 */
sdb_errno MqttInit(comm_protocol_api *MQTT);

/**
 * @brief Runs MQTT protocol handler
 * @todo Add termination logic
 *
 * @param MQTT Protocol API structure
 * @return sdb_errno Success/error code
 */
sdb_errno MqttRun(comm_protocol_api *MQTT);

/**
 * @brief Finalizes MQTT protocol handler
 *
 * @param MQTT Protocol API structure
 * @return sdb_errno Success/error code
 */
sdb_errno MqttFinalize(comm_protocol_api *MQTT);

SDB_END_EXTERN_C

#endif // MQTT_H
