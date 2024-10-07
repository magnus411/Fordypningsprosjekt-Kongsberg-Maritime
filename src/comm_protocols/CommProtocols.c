#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <errno.h>
#include <SdbExtern.h>

#include <comm_protocols/CommProtocols.h>

#include <comm_protocols/Modbus.h>
#include <comm_protocols/MQTT.h>

static sdb_errno ModbusInitialize(comm_protocol_api *ProtocolApi, protocol_args *Args);
static sdb_errno ModbusStartComm(comm_protocol_api *ProtocolApi);
static sdb_errno ModbusCleanup(comm_protocol_api *ProtocolApi);

static sdb_errno MQTTInitialize(comm_protocol_api *ProtocolApi, protocol_args *Args);
static sdb_errno MQTTStartComm(comm_protocol_api *ProtocolApi);
static sdb_errno MQTTCleanup(comm_protocol_api *ProtocolApi);

sdb_errno
PickProtocol(protocol_type Type, comm_protocol_api *ProtocolApi)
{
    if(ProtocolApi == NULL) {
        return -EINVAL;
    }

    memset(ProtocolApi, 0, sizeof(comm_protocol_api));

    switch(Type) {
        case PROTOCOL_MODBUS_TCP:
            ProtocolApi->Initialize = ModbusInitialize;
            ProtocolApi->StartComm  = ModbusStartComm;
            ProtocolApi->Cleanup    = ModbusCleanup;
            break;
        case PROTOCOL_MQTT:
            ProtocolApi->Initialize = MQTTInitialize;
            ProtocolApi->StartComm  = MQTTStartComm;
            ProtocolApi->Cleanup    = MQTTCleanup;
            break;
        default:
            return -ENOTSUP;
    }

    atomic_init(&ProtocolApi->IsInitialized, false);
    return 0;
}
static sdb_errno
ModbusInitialize(comm_protocol_api *ProtocolApi, protocol_args *Args)
{
    if(ProtocolApi == NULL || Args == NULL || Args->Cb == NULL) {
        return -EINVAL;
    }

    modbus_args *ModbusArgs = malloc(sizeof(modbus_args));
    if(ModbusArgs == NULL) {
        return -ENOMEM;
    }

    ModbusArgs->PORT = Args->Port;
    strncpy(ModbusArgs->Ip, Args->Address, sizeof(ModbusArgs->Ip) - 1);
    ModbusArgs->Ip[sizeof(ModbusArgs->Ip) - 1] = '\0';
    ModbusArgs->Cb                             = Args->Cb;

    ProtocolApi->Context = ModbusArgs;
    atomic_store(&ProtocolApi->IsInitialized, true);
    return 0;
}

static sdb_errno
ModbusStartComm(comm_protocol_api *ProtocolApi)
{
    if(ProtocolApi == NULL || !atomic_load(&ProtocolApi->IsInitialized)) {
        return -EINVAL;
    }

    //! (Magnus): Should we create a thread here, or should it be done outside chen calling this?
    modbus_args *ModbusArgs = (modbus_args *)ProtocolApi->Context;
    pthread_t    Thread;
    int          Result = pthread_create(&Thread, NULL, ModbusThread, ModbusArgs);
    if(Result != 0) {
        return -Result;
    }
    pthread_detach(Thread);
    return 0;
}

static sdb_errno
ModbusCleanup(comm_protocol_api *ProtocolApi)
{
    if(ProtocolApi == NULL || !atomic_load(&ProtocolApi->IsInitialized)) {
        return -EINVAL;
    }

    free(ProtocolApi->Context);
    ProtocolApi->Context = NULL;
    atomic_store(&ProtocolApi->IsInitialized, false);
    return 0;
}

static sdb_errno
MQTTInitialize(comm_protocol_api *ProtocolApi, protocol_args *Args)
{
    if(ProtocolApi == NULL || Args == NULL || Args->Cb == NULL) {
        return -EINVAL;
    }

    mqtt_subscriber *MqttArgs = malloc(sizeof(mqtt_subscriber));
    if(MqttArgs == NULL) {
        return -ENOMEM;
    }

    sdb_errno Result
        = InitSubscriber(MqttArgs, Args->Address, "ClientID", Args->Topic, Args->Qos, Args->Cb);
    if(Result != 0) {
        free(MqttArgs);
        return Result;
    }

    ProtocolApi->Context = MqttArgs;
    atomic_store(&ProtocolApi->IsInitialized, true);
    return 0;
}

static sdb_errno
MQTTStartComm(comm_protocol_api *ProtocolApi)
{
    if(ProtocolApi == NULL || !atomic_load(&ProtocolApi->IsInitialized)) {
        return -EINVAL;
    }

    mqtt_subscriber *MqttArgs = (mqtt_subscriber *)ProtocolApi->Context;
    pthread_t        Thread;
    int              Result = pthread_create(&Thread, NULL, MQTTSubscriberThread, MqttArgs);
    if(Result != 0) {
        return -Result;
    }
    pthread_detach(Thread);
    return 0;
}

static sdb_errno
MQTTCleanup(comm_protocol_api *ProtocolApi)
{
    if(ProtocolApi == NULL || !atomic_load(&ProtocolApi->IsInitialized)) {
        return -EINVAL;
    }

    mqtt_subscriber *MqttArgs = (mqtt_subscriber *)ProtocolApi->Context;
    free(MqttArgs->Address);
    free(MqttArgs->ClientId);
    free(MqttArgs->Topic);
    free(MqttArgs);
    ProtocolApi->Context = NULL;
    atomic_store(&ProtocolApi->IsInitialized, false);
    return 0;
}
