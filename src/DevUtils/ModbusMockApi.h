#ifndef MODBUS_MOCK_API_H
#define MODBUS_MOCK_API_H
#include <src/Sdb.h>

#include <src/CommProtocols/CommProtocols.h>

sdb_errno MbMockInit(comm_protocol_api *Mb);
sdb_errno MbMockRun(comm_protocol_api *Mb);
sdb_errno MbMockFinalize(comm_protocol_api *Mb);
sdb_errno MbMockApiInit(Comm_Protocol_Type Type, sdb_thread_control *ModuleControl, u64 SensorCount,
                        sensor_data_pipe **SdPipes, sdb_arena *Arena, u64 ArenaSize, i64 CommTId,
                        comm_protocol_api *CpApi);

#endif
