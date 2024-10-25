#include <errno.h>
#include <pthread.h>

#include <src/Sdb.h>
SDB_LOG_REGISTER(CommProtocols);

#include <src/CommProtocols/CommProtocols.h>
#include <src/CommProtocols/MQTT.h>
#include <src/CommProtocols/Modbus.h>
#include <src/Common/SensorDataPipe.h>

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
CpProtocolIsAvailable(Comm_Protocol_Type Type)
{
    switch(Type) {
        case Comm_Protocol_Modbus_TCP:
            return ModbusAvailable_;
        case Comm_Protocol_MQTT:
            return MQTTAvailable_;
        default:
            SdbAssert(0, "Invalid protocol type");
            return false;
    }
}

sdb_errno
CpInitApi(Comm_Protocol_Type Type, sensor_data_pipe *SdPipe, sdb_arena *Arena, u64 ArenaSize,
          i64 CommTId, comm_protocol_api *CpApi)
{
    if(!CpProtocolIsAvailable(Type)) {
        SdbLogWarning("%s is unavailable", CpTypeToName(Type));
        return -SDBE_CP_UNAVAIL;
    }

    SdbMemset(CpApi, 0, sizeof(*CpApi));
    SdbMemcpy(&CpApi->SdPipe, SdPipe, sizeof(*SdPipe));
    SdbArenaBootstrap(Arena, &CpApi->Arena, ArenaSize);

    switch(Type) {
        case Comm_Protocol_Modbus_TCP:
            {
                CpApi->Init     = ModbusInit;
                CpApi->Run      = ModbusRun;
                CpApi->Finalize = ModbusFinalize;
                CpApi->OptArgs  = SdbStrdup("127.0.0.1", Arena);
            }
            break;
        case Comm_Protocol_MQTT:
            {
                CpApi->Init     = MqttInit;
                CpApi->Run      = MqttRun;
                CpApi->Finalize = MqttFinalize;
                CpApi->OptArgs  = (void *)CommTId;
            }
            break;
        default:
            SdbAssert(0, "Comm protocol does not exist");
            return -EINVAL;
    }

    return 0;
}
