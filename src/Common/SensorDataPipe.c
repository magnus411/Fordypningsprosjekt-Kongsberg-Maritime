#include <stdio.h>

#include <src/Sdb.h>
SDB_LOG_REGISTER(SensorDataPipe);

#include <src/Common/CircularBuffer.h>
#include <src/Common/SensorDataPipe.h>

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

ssize_t
SdPipeInsert(sensor_data_pipe *SdPipe, u64 Buf, void *Data, size_t Size)
{
    SdbAssert(Buf < SdPipe->BufCount, "Sensor data pipe does not have buffer #%lu", Buf);
    ssize_t Ret = CbInsert(&SdPipe->Buffers[Buf], Data, Size);
    return Ret;
}

ssize_t
SdPipeRead(sensor_data_pipe *SdPipe, u64 Buf, void *To, size_t Size)
{
    SdbAssert(Buf < SdPipe->BufCount, "Sensor data pipe does not have buffer #%lu", Buf);
    ssize_t Ret = CbRead(&SdPipe->Buffers[Buf], To, Size);
    return Ret;
}
