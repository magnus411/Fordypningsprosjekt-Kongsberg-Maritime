#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include <errno.h>
#include <SdbExtern.h>

#include <comm_protocols/CommProtocols.h>

#include <comm_protocols/Modbus.h>
#include <comm_protocols/MQTT.h>

#if COMM_PROTOCOL_MODBUS == 1
static bool ModbusAvailable_ = true;
#else
static bool ModbusAvailable_ = false;
#endif

#if COMM_PROTOCOL_MQTT == 1
static bool MQTTAvailable_ = true;
#else
static bool MQTTAvailable_ = false;
#endif

bool
CpProtocolIsAvailable(Protocol_Type ProtocolId)
{
    switch(ProtocolId) {
        case Protocol_Modbus_TCP:
            return ModbusAvailable_;
        case Protocol_MQTT:
            return MQTTAvailable_;
        default:
            return false;
    }
}

sdb_errno
PickProtocol(Protocol_Type Type, comm_protocol_api *ProtocolApi)
{
    if(ProtocolApi == NULL) {
        return -EINVAL;
    }

    if(CpProtocolIsAvailable(Type) == false) {
        return -ENOTSUP;
    }

    memset(ProtocolApi, 0, sizeof(comm_protocol_api));

    switch(Type) {
        case Protocol_Modbus_TCP:

            ProtocolApi->Initialize = ModbusInitialize;
            ProtocolApi->StartComm  = ModbusStartComm;
            ProtocolApi->Cleanup    = ModbusCleanup;
            break;
        case Protocol_MQTT:

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
