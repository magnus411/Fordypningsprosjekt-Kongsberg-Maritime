#ifndef MODBUS_API_H
#define MODBUS_API_H

#include <src/Sdb.h>

#include <src/CommProtocols/CommProtocols.h>

sdb_errno MbInit(comm_protocol_api *Mb);
sdb_errno MbRun(comm_protocol_api *Mb);
sdb_errno MbFinalize(comm_protocol_api *Mb);

#endif
