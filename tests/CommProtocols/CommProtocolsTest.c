
#include <src/Sdb.h>
SDB_LOG_REGISTER(CommProtocolsTest);

#include <src/CommProtocols/CommProtocols.h>
#include <src/Common/SensorDataPipe.h>

#include <tests/CommProtocols/CommProtocolsTest.h>
#include <tests/CommProtocols/MQTTTest.h>
#include <tests/CommProtocols/ModbusTest.h>


sdb_errno
CpInitApiTest(Comm_Protocol_Type Type, sensor_data_pipe *SdPipe, sdb_arena *Arena, u64 ArenaSize,
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
                CpApi->Init     = ModbusInitTest;
                CpApi->Run      = ModbusRunTest;
                CpApi->Finalize = ModbusFinalizeTest;
                CpApi->OptArgs  = SdbStrdup("127.0.0.1", Arena);
            }
            break;
        case Comm_Protocol_MQTT:
            {
                CpApi->Init     = MqttInitTest;
                CpApi->Run      = MqttRunTest;
                CpApi->Finalize = MqttFinalizeTest;
                CpApi->OptArgs  = (void *)CommTId;
            }
            break;
        default:
            SdbAssert(0, "Comm protocol does not exist");
            return -EINVAL;
    }

    return 0;
}
