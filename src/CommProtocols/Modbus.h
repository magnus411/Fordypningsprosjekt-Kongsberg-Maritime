#ifndef MODBUS_H
#define MODBUS_H

#include <src/Sdb.h>

SDB_BEGIN_EXTERN_C

#include <src/Common/SensorDataPipe.h>
#include <src/Common/Thread.h>

// NOTE(ingar): This is port (502) used by modbus according to its wikipedia page, but I can't get
// it to work for testing, since it's in the reserved range.
#define MODBUS_PORT                   (1312) // (502)
#define MODBUS_TCP_HEADER_LEN         (7)
#define MODBUS_PDU_MAX_SIZE           (253)
#define MODBUS_TCP_FRAME_MAX_SIZE     (260)
#define MODBUS_READ_HOLDING_REGISTERS (0x03)

typedef struct
{
    int        SockFd;
    int        Port;
    sdb_string Ip;

} mb_conn;

typedef struct
{
    u64      ConnCount;
    mb_conn *Conns;
    // NOTE(ingar): Keep string last so it's allocated contiguously with the context

} modbus_ctx;

typedef struct
{
    u64  PortCount;
    int *Ports;

    u64         IpCount;
    sdb_string *Ips;

} mb_init_args;

#ifndef MB_SCRATCH_COUNT
#define MB_SCRATCH_COUNT 2
#endif

void        MbThreadArenasInit(void);
const u8   *MbParseTcpFrame(const u8 *Frame, u16 *UnitId, u16 *DataLength);
modbus_ctx *MbPrepareCtx(sdb_arena *MbArena);

SDB_END_EXTERN_C

#endif
