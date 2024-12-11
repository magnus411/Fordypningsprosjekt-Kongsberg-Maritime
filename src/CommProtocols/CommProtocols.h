#ifndef COMM_PROTOCOLS_H
#define COMM_PROTOCOLS_H

#include <src/Sdb.h>

SDB_BEGIN_EXTERN_C

#include <src/Common/SensorDataPipe.h>

typedef enum
{
    Comm_Protocol_Modbus_TCP,
    Comm_Protocol_MQTT

} Comm_Protocol_Type;

static inline const char *
CpTypeToName(Comm_Protocol_Type Type)
{
    switch(Type) {
        case Comm_Protocol_Modbus_TCP:
            return "Modbus TCP";
        case Comm_Protocol_MQTT:
            return "MQTT";
        default:
            return "Protocol does not exist";
    }
}

typedef struct comm_protocol_api comm_protocol_api;
struct comm_protocol_api
{
    sdb_errno (*Init)(comm_protocol_api *Cp);
    sdb_errno (*Run)(comm_protocol_api *Cp);
    sdb_errno (*Finalize)(comm_protocol_api *Cp);

    sdb_thread_control *ModuleControl;
    // NOTE(ingar): Passed in to stay in the api run function instead of returning to the module
    // code

    u64                SensorCount;
    sensor_data_pipe **SdPipes;

    void *Ctx;

    sdb_arena Arena;
};

typedef sdb_errno (*cp_init_api)(Comm_Protocol_Type Type, sdb_thread_control *ModuleControl,
                                 u64 SensorCount, sensor_data_pipe **SdPipes, sdb_arena *Arena,
                                 u64 ArenaSize, i64 CommTId, comm_protocol_api *CpApi);

typedef struct
{
    sdb_barrier *ModulesBarrier;

    Comm_Protocol_Type CpType;
    cp_init_api        InitApi;

    u64                SensorCount;
    sensor_data_pipe **SdPipes; // TODO(ingar): 1 pipe per sensor?

    sdb_thread_control Control;

    u64       CpArenaSize;
    u64       ArenaSize;
    sdb_arena Arena;

} comm_module_ctx;

bool CpProtocolIsAvailable(Comm_Protocol_Type Type);

sdb_errno CpApiInit(Comm_Protocol_Type Type, sdb_thread_control *ModuleControl, u64 SensorCount,
                    sensor_data_pipe **SdPipes, sdb_arena *Arena, u64 ArenaSize, i64 CommTId,
                    comm_protocol_api *CpApi);

comm_module_ctx *CommModulePrepare(sdb_barrier *ModulesBarrier, Comm_Protocol_Type Type,
                                   cp_init_api ApiInit, sensor_data_pipe **Pipes, u64 SensorCount,
                                   u64 ModuleArenaSize, u64 DbsArenaSize, sdb_arena *Arena);

// sdb_errno CommModuleRun(sdb_thread *CommThread);


SDB_END_EXTERN_C

#endif // COMM_PROTOCOLS_H
