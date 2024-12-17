/**
 * @file Modbus.c
 * @brief Implementation of Modbus protocol handling
 *
 * Resources:
 * Modbus Application Protocol. Defines the Modbus spesifications.
 * @link https://modbus.org/docs/Modbus_Application_Protocol_V1_1b.pdf

 */
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <src/Sdb.h>
SDB_LOG_REGISTER(Modbus);
SDB_THREAD_ARENAS_REGISTER(Modbus, 2);

#include <src/CommProtocols/Modbus.h>
#include <src/Common/SensorDataPipe.h>
#include <src/Common/Socket.h>
#include <src/Common/Thread.h>


void
MbThreadArenasInit(void)
{
    SdbThreadArenasInit(Modbus);
}

/**
 * @brief Receives a complete Modbus TCP frame
 *
 * Implements a two-stage reading process:
 * 1. Reads the header to determine frame size
 * 2. Reads the remaining data
 *
 * @param Sockfd Socket file descriptor
 * @param Frame Buffer to store received frame
 * @param BufferSize Size of provided buffer
 * @return Number of bytes read or negative on error
 */
ssize_t
MbReceiveTcpFrame(int Sockfd, u8 *Frame, size_t BufferSize)
{
    ssize_t TotalBytesRead = 0;

    while(TotalBytesRead < MODBUS_TCP_HEADER_LEN) {
        ssize_t BytesRead
            = recv(Sockfd, Frame + TotalBytesRead, MODBUS_TCP_HEADER_LEN - TotalBytesRead, 0);
        if(BytesRead <= 0) {
            return BytesRead;
        }
        TotalBytesRead += BytesRead;
    }

    u16 Length = (Frame[4] << 8) | Frame[5]; // Length from Modbus TCP header

    ssize_t TotalFrameSize = MODBUS_TCP_HEADER_LEN + Length;

    if(TotalFrameSize > BufferSize) {
        SdbLogDebug("Frame too large for buffer. Total frame size: %zd\n", TotalFrameSize);
        return -1;
    }

    while(TotalBytesRead < TotalFrameSize) {
        ssize_t BytesRead
            = recv(Sockfd, Frame + TotalBytesRead, TotalFrameSize - TotalBytesRead, 0);
        if(BytesRead <= 0) {
            return BytesRead;
        }
        TotalBytesRead += BytesRead;
    }

    return TotalBytesRead;
}

/**
 * @brief Parses a Modbus TCP frame into its components
 *
 * Frame Structure:
 * ~~~~~~~~
 * | Field         | Size    | Description            |
 * |---------------|---------|------------------------|
 * | Transaction ID| 2 bytes | Request identifier     |
 * | Protocol ID   | 2 bytes | Protocol (0 = Modbus)  |
 * | Length        | 2 bytes | Number of following bytes|
 * | Unit ID       | 1 byte  | Slave identifier       |
 * | Function Code | 1 byte  | Action code           |
 * | DataLength    | 1 byte  | Length of data field  |
 * | Data          | n bytes | Payload data          |
 * ~~~~~~~~
 *
 * The function checks for frame consistency by verifying that the Length field
 * matches DataLength + 3 (accounting for UnitID, FunctionCode, and ByteCount).
 *
 * @param Frame Pointer to the raw Modbus TCP frame
 * @param UnitId Pointer to store the parsed Unit ID
 * @param DataLength Pointer to store the data length
 *
 * @return Pointer to the start of data section, NULL if parsing fails or frame is invalid
 *
 * @see https://en.wikipedia.org/wiki/Modbus#Public_function_codes
 * @see https://modbus.org/docs/Modbus_Application_Protocol_V1_1b.pdf
 *
 * @note Bytes are arranged in big-endian order (most significant byte first)
 */
const u8 *
MbParseTcpFrame(const u8 *Frame, u16 *UnitId, u16 *DataLength)
{
    u16 Length = (Frame[4] << 8) | Frame[5];

    *UnitId = Frame[6];

    *DataLength = Frame[8];

    if(Length != *DataLength + 3) {
        SdbLogWarning("Inconsistent Modbus frame lengths: Length=%u, DataLength=%u", Length,
                      *DataLength);
        return NULL;
    }

    return &Frame[9];
}


/**
 * @brief Prepares Modbus context from configuration file
 *
 * Reads IP address and port from configuration file and initializes
 * connections. Creates and configures sockets for each connection.
 *
 * @param MbArena Memory arena for allocations
 * @return Initialized context or NULL on failure
 */
modbus_ctx *
MbPrepareCtx(sdb_arena *MbArena)
{
    modbus_ctx *MbCtx = SdbPushStruct(MbArena, modbus_ctx);
    MbCtx->ConnCount  = 1;
    MbCtx->Conns      = SdbPushArray(MbArena, mb_conn, MbCtx->ConnCount);


    sdb_scratch_arena Scratch = SdbScratchGet(NULL, 0);
    if(!Scratch.Arena) {
        SdbLogError("Failed to get scratch arena");
        return NULL;
    }


    sdb_file_data *ConfFile = SdbLoadFileIntoMemory(MODBUS_CONF_FS_PATH, MbArena);
    if(ConfFile == NULL) {
        SdbLogError("Failed to open config file");
        SdbScratchRelease(Scratch);
        return NULL;
    }

    char *Content = (char *)ConfFile->Data;
    char *IpAddr  = NULL;
    int   Port    = -1;

    char *Line = strtok(Content, "\n");
    while(Line != NULL) {
        if(strncmp(Line, "ipv4Addr=", 9) == 0) {
            IpAddr = Line + 9;
        } else if(strncmp(Line, "port=", 5) == 0) {
            Port = atoi(Line + 5);
        }
        Line = strtok(NULL, "\n");
    }

    if(IpAddr == NULL || Port == -1) {
        SdbLogError("Failed to parse IP or port from config file");
        SdbScratchRelease(Scratch);
        return NULL;
    }


    mb_conn *Conns = MbCtx->Conns;
    for(u64 i = 0; i < MbCtx->ConnCount; ++i) {
        Conns[i].Port = Port;
        Conns[i].Ip   = SdbStringMake(MbArena, IpAddr);

        Conns[i].SockFd = SocketCreate(Conns[i].Ip, Conns[i].Port);
        if(Conns[i].SockFd == -1) {
            SdbLogError("Failed to create socket for sensor index %lu", i);
            return NULL;
        } else {
            SdbLogDebug("Modbus thread %lu successfully connected to server %s:%d", i, Conns[i].Ip,
                        Conns[i].Port);
        }
    }

    return MbCtx;
}
