#include <src/Sdb.h>

SDB_LOG_DECLARE(Modbus);
SDB_THREAD_ARENAS_EXTERN(Modbus);

#include <src/CommProtocols/CommProtocols.h>
#include <src/CommProtocols/Modbus.h>

#if 0
sdb_errno
MbInit(comm_protocol_api *Modbus)
{

    modbus_ctx *Ctx = SdbPushStruct(&Modbus->Arena, modbus_ctx);
    Ctx->PORT       = MODBUS_PORT;
    strncpy(Ctx->Ip, (char *)Modbus->OptArgs, 10);
    Ctx->SockFd = CreateSocket(Ctx->Ip, Ctx->PORT);

    if(Ctx->SockFd == -1) {
        SdbLogError("Failed to create socket");
        return -1;
    }

    Modbus->Ctx = Ctx;

    return 0;
}

sdb_errno
MbRun(comm_protocol_api *Modbus)
{
    modbus_ctx *Ctx = Modbus->Ctx;
    u8          Buf[MAX_MODBUS_TCP_FRAME];
    while(true) {
        ssize_t NumBytes = MbReceiveTcpFrame(Ctx->SockFd, Buf, sizeof(Buf));
        if(NumBytes > 0) {
            queue_item Item;
            MbParseTcpFrame(Buf, NumBytes, &Item);
            MetricAddSample(&InputThroughput, NumBytes);

            // SdPipeInsert(&Modbus->SdPipe, 0, &Item, sizeof(queue_item));
        } else if(NumBytes == 0) {
            SdbLogDebug("Connection closed by server");
            return SDBE_CONN_CLOSED_SUCS;
        } else {
            SdbLogError("Error during read operattion. Closing connection");
            return -SDBE_CONN_CLOSED_ERR;
        }
    }

    return 0;
}

sdb_errno
MbFinalize(comm_protocol_api *Modbus)
{
    close(MB_CTX(Modbus)->SockFd);
    SdbArenaClear(&Modbus->Arena);
    return 0;
}
#else
sdb_errno
MbInit(comm_protocol_api *Mb)
{
    return 0;
}

sdb_errno
MbRun(comm_protocol_api *Mb)
{
    return 0;
}

sdb_errno
MbFinalize(comm_protocol_api *Mb)
{
    return 0;
}
#endif
