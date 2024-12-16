#ifndef SOCKETMODULE_H
#define SOCKETMODULE_H


/**
 * @file Socket.h
 * @brief Simplified Socket Communication Utilities
 *
 * Provides high-level, error-handled socket creation and
 * receive operations with timeout support.
 *
 * Key Features:
 * - IPv4 TCP socket creation
 * - Connection establishment with error handling
 * - Receive operation with configurable timeout
 *
 * Design Principles:
 * - Robust error reporting
 * - Simplified socket management
 * - Timeout-based receive mechanism
 *
 * @note Uses standard POSIX socket interfaces
 */


#include <src/Sdb.h>

#include <src/Common/Time.h>

SDB_BEGIN_EXTERN_C

#include <sys/socket.h>
#include <sys/types.h>


/**
 * @brief Create and Connect a TCP Socket
 *
 * Establishes a TCP connection to specified IP and port.
 *
 * @param IpAddress Destination IP address (IPv4 dot-decimal notation)
 * @param Port Destination port number
 *
 * @return int
 * - Socket file descriptor on success
 * - -1 on connection failure
 *
 * @note Includes a 1-second connection delay for stability
 */
int SocketCreate(const char *IpAddress, int Port);


/**
 * @brief Receive Data with Timeout
 *
 * Attempts to receive data from a socket with a specified timeout.
 *
 * @param SockFd Socket file descriptor
 * @param Buffer Destination buffer for received data
 * @param Length Maximum number of bytes to receive
 * @param Timeout Maximum time to wait for data
 *
 * @return int
 * - >0: Number of bytes received
 * - 0: Connection closed by server
 * - -1: Socket error
 * - -2: Receive timeout
 */
int SocketRecvWithTimeout(int SockFd, void *Buffer, size_t Length, sdb_timediff Timeout);

SDB_END_EXTERN_C

#endif /* SOCKETMODULE_H */
