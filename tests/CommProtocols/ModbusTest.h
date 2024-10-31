#ifndef MODBUS_SOCKET_TEST_H
#define MODBUS_SOCKET_TEST_H

#include <src/Common/Thread.h>

#include <src/Sdb.h>

#include <src/CommProtocols/CommProtocols.h>
#include <src/Common/SensorDataPipe.h>

// NOTE(ingar): 502 is the port used by modbus according to its wikipedia page, though I get denied
// access if I use it
#define MODBUS_PORT (54321) // (502)

sdb_errno RunModbusTestServer(sdb_thread *Thread);

sdb_errno MbInitTest(comm_protocol_api *Modbus);
sdb_errno MbRunTest(comm_protocol_api *Modbus);
sdb_errno MbFinalizeTest(comm_protocol_api *Modbus);


#endif
