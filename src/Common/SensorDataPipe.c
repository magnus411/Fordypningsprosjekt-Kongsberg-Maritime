#include <src/Sdb.h>

#include <src/Common/CircularBuffer.h>
#include <src/Common/SensorDataPipe.h>

SDB_LOG_REGISTER(SensorDataPipe);

sdb_errno
SdPipeInit(sensor_data_pipe *SdPipe, u64 BufCount, size_t *BufSizes, sdb_arena *Arena)
{
    SdPipe->BufCount = BufCount;
    SdPipe->BufSizes = SdbPushArray(Arena, size_t, BufCount);
    SdPipe->Buffers  = SdbPushArray(Arena, circular_buffer, BufCount);
    SdbMemcpy(SdPipe->BufSizes, BufSizes, BufCount);

    sdb_errno CbInitRet = 0;
    for(i64 i = 0; i < BufCount; ++i) {
        if((CbInitRet = CbInit(&SdPipe->Buffers[i], BufSizes[i], Arena)) != 0) {
            SdbLogError("Failed to init pipe buffer %ld", i);
            return CbInitRet;
        }
    }

    SdbLogInfo("Sensor data pipe initialized successfully");
    return 0;
}
