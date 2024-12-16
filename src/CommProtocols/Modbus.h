/**
 * @file Modbus.h
 * @brief Modbus protocol implementation interface
 * @details Provides functionality for Modbus TCP communication including frame parsing,
 * context management, and connection handling. This implementation follows the
 * Modbus Application Protocol Specification V1.1b.
 */

#ifndef MODBUS_H
#define MODBUS_H

#include <src/Sdb.h>

SDB_BEGIN_EXTERN_C

#include <src/Common/SensorDataPipe.h>
#include <src/Common/Thread.h>

/**
 * @brief Default Modbus TCP port (testing port, standard is 502)
 * @note Using non-standard port as 502 is in reserved range
 */
#define MODBUS_PORT (1312)

/** @brief Length of Modbus TCP header in bytes */
#define MODBUS_TCP_HEADER_LEN (7)

/** @brief Maximum size of Modbus Protocol Data Unit */
#define MODBUS_PDU_MAX_SIZE (253)

/** @brief Maximum size of complete Modbus TCP frame */
#define MODBUS_TCP_FRAME_MAX_SIZE (260)

/** @brief Function code for reading holding registers */
#define MODBUS_READ_HOLDING_REGISTERS (0x03)

/** @brief Path to Modbus configuration file */
#define MODBUS_CONF_FS_PATH "./configs/modbus-conf"

/**
 * @struct mb_conn
 * @brief Represents a single Modbus TCP connection
 */
typedef struct
{
    int        SockFd; /**< Socket file descriptor */
    int        Port;   /**< Connection port number */
    sdb_string Ip;     /**< IP address string */
} mb_conn;

/**
 * @struct modbus_ctx
 * @brief Context structure for Modbus operations
 */
typedef struct
{
    u64      ConnCount; /**< Number of active connections */
    mb_conn *Conns;     /**< Array of connection structures */
    // NOTE(ingar): Keep string last so it's allocated contiguously with the context
} modbus_ctx;

/**
 * @struct mb_init_args
 * @brief Initialization arguments for Modbus connections
 */
typedef struct
{
    u64         PortCount; /**< Number of ports */
    int        *Ports;     /**< Array of port numbers */
    u64         IpCount;   /**< Number of IP addresses */
    sdb_string *Ips;       /**< Array of IP address strings */
} mb_init_args;

/** @brief Number of scratch arenas for Modbus operations */
#ifndef MB_SCRATCH_COUNT
#define MB_SCRATCH_COUNT 2
#endif

/**
 * @brief Initializes thread arenas for Modbus operations
 */
void MbThreadArenasInit(void);

/**
 * @brief Parses a Modbus TCP frame
 *
 * @param Frame Raw frame data
 * @param UnitId Pointer to store unit ID
 * @param DataLength Pointer to store data length
 * @return Pointer to start of data section, NULL if parsing fails
 */
const u8 *MbParseTcpFrame(const u8 *Frame, u16 *UnitId, u16 *DataLength);

/**
 * @brief Prepares Modbus context from configuration
 *
 * @param MbArena Memory arena for allocations
 * @return Initialized modbus context, NULL on failure
 */
modbus_ctx *MbPrepareCtx(sdb_arena *MbArena);

SDB_END_EXTERN_C

#endif