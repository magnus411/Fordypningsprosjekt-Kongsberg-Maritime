#ifndef COMM_MODULE_H
#define COMM_MODULE_H

#include <src/Sdb.h>

#include <src/CommProtocols/CommProtocols.h>
#include <src/Common/SensorDataPipe.h>

typedef struct
{
    i64                ThreadId;
    sdb_errno          Errno;
    Comm_Protocol_Type CpType;

    sensor_data_pipe SdPipe;

    sdb_arena Arena;
    u64       ArenaSize;
    u64       CpArenaSize;

} comm_module_ctx;

void *CommModuleRun(void *CommModuleCtx);

#endif
