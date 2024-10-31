#ifndef COMM_PROTOCOL_TEST_H
#define COMM_PROTOCOL_TEST_H

#include <src/Sdb.h>

#include <src/CommProtocols/CommProtocols.h>
#include <src/Common/SensorDataPipe.h>

sdb_errno CpTestApiInit(Comm_Protocol_Type Type, sdb_thread_control *ModuleControl, u64 SensorCount,
                        sensor_data_pipe **SdPipes, sdb_arena *Arena, u64 ArenaSize, i64 CommTId,
                        comm_protocol_api *CpApi);


#endif
