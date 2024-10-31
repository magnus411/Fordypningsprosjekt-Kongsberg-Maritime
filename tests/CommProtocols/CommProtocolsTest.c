
#include <src/Sdb.h>
SDB_LOG_REGISTER(CommProtocolsTest);

#include <src/CommProtocols/CommProtocols.h>
#include <src/CommProtocols/MQTT.h>
#include <src/CommProtocols/Modbus.h>
#include <src/Common/SensorDataPipe.h>

#include <tests/CommProtocols/CommProtocolsTest.h>
#include <tests/CommProtocols/MQTTTest.h>
#include <tests/CommProtocols/ModbusTest.h>


sdb_errno
CpInitApiTest(Comm_Protocol_Type Type, u64 SensorCount, sensor_data_pipe **SdPipes,
              sdb_arena *Arena, u64 ArenaSize, i64 CommTId, comm_protocol_api *CpApi)
{
    if(!CpProtocolIsAvailable(Type)) {
        SdbLogWarning("%s is unavailable", CpTypeToName(Type));
        return -SDBE_CP_UNAVAIL;
    }

    SdbMemset(CpApi, 0, sizeof(*CpApi));
    CpApi->SensorCount = SensorCount;
    CpApi->SdPipes     = SdPipes;
    SdbArenaBootstrap(Arena, &CpApi->Arena, ArenaSize);

    switch(Type) {
        case Comm_Protocol_Modbus_TCP:
            {
                CpApi->Init     = MbInitTest;
                CpApi->Run      = MbRunTest;
                CpApi->Finalize = MbFinalizeTest;

                mb_init_args *Args = SdbPushStruct(&CpApi->Arena, mb_init_args);
                Args->Ips          = SdbPushArray(&CpApi->Arena, sdb_string, 1);
                Args->Ips[0]       = SdbStringMake(&CpApi->Arena, "127.0.0.1");
                Args->Ports        = SdbPushArray(&CpApi->Arena, int, 1);
                Args->Ports[0]     = MODBUS_PORT;
                CpApi->OptArgs     = Args;
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
