#ifndef MODBUS_TEST_SERVER_H
#define MODBUS_TEST_SERVER_H

#include <src/Common/Thread.h>
#include <src/Sdb.h>

// NOTE(ingar): 502 is the port used by modbus according to its wikipedia page, though I get denied
// access if I use it
#define MODBUS_PORT (50123) // (502)

void RunModbusTestServer(sdb_barrier *Barrier);

#endif
