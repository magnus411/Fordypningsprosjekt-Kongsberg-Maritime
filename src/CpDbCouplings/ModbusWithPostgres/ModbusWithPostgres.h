#ifndef MB_W_PG_COUPLING_H
#define MB_W_PG_COUPLING_H
#include <src/Sdb.h>

#include <src/Common/SensorDataPipe.h>
#include <src/Common/ThreadGroup.h>

#include <src/Libs/cJSON/cJSON.h>

typedef struct
{
    u64 ModbusMemSize;
    u64 ModbusScratchSize;
    u64 PgMemSize;
    u64 PgScratchSize;

    sensor_data_pipe *SdPipe;
    sdb_barrier       Barrier;

} mbpg_ctx;

void *PgThread(void *Arg);

void *MbThread(void *Arg);

sdb_errno MbPgCleanup(void *Arg);

tg_group *MbPgCreateTg(cJSON *Conf, u64 GroupId, sdb_arena *A);

#endif
