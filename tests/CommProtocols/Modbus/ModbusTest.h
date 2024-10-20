#ifndef MODBUS_SOCKET_TEST_H
#define MODBUS_SOCKET_TEST_H

#include <src/Common/Errno.h>
#include <src/Common/Thread.h>

sdb_errno RunModbusTestServer(sdb_thread *Thread);

#endif
