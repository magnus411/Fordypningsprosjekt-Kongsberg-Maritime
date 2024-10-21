#ifndef COMM_MODULE_H
#define COMM_MODULE_H

#include <src/Sdb.h>

#include <src/CommProtocols/CommProtocols.h>
#include <src/Common/SensorDataPipe.h>
#include <src/Common/Thread.h>

typedef struct
{
    sdb_barrier *ModulesBarrier;

    Comm_Protocol_Type CpType;
    cp_init_api        InitApi;

    sensor_data_pipe SdPipe;

    sdb_arena Arena;
    u64       ArenaSize;
    u64       CpArenaSize;

} comm_module_ctx;

sdb_errno CommModuleRun(sdb_thread *CommThread);

#endif
