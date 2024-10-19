#ifndef COMM_PROTOCOLS_H
#define COMM_PROTOCOLS_H

#include <src/Sdb.h>

#include <src/Common/SensorDataPipe.h>

typedef enum
{
    Comm_Protocol_Modbus_TCP,
    Comm_Protocol_MQTT

} Comm_Protocol_Type;

typedef struct comm_protocol_api comm_protocol_api;
struct comm_protocol_api
{
    sdb_errno (*Init)(comm_protocol_api *Cp, void *OptArgs);
    sdb_errno (*Run)(comm_protocol_api *Cp);
    sdb_errno (*Finalize)(comm_protocol_api *Cp);

    sensor_data_pipe SdPipe;
    sdb_arena        Arena;
    void            *Ctx;
};

sdb_errno CpInitApi(Comm_Protocol_Type Type, sensor_data_pipe *SdPipe, sdb_arena *Arena,
                    u64 ArenaSize, i64 CommTId, comm_protocol_api *CpApi, void *OptArgs);

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

#endif // COMM_PROTOCOLS_H
