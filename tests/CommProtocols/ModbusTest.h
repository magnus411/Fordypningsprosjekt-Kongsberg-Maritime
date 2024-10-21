#ifndef MODBUS_SOCKET_TEST_H
#define MODBUS_SOCKET_TEST_H

#include <src/Common/Thread.h>

#include <src/Sdb.h>

#include <src/CommProtocols/CommProtocols.h>
#include <src/Common/SensorDataPipe.h>

// NOTE(ingar): This is port used by modbus according to its wikipedia page
#define MODBUS_PORT (3490) // (502)

sdb_errno RunModbusTestServer(sdb_thread *Thread);

sdb_errno ModbusInitTest(comm_protocol_api *Modbus);
sdb_errno ModbusRunTest(comm_protocol_api *Modbus);
sdb_errno ModbusFinalizeTest(comm_protocol_api *Modbus);


#endif
