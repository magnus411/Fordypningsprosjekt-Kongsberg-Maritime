#ifndef SOCKETMODULE_H
#define SOCKETMODULE_H

#include <sys/socket.h>
#include <sys/types.h>

/***************************************************
 * @file SocketModule.c
 * @brief Socket Module
 * @author Gutta Boys
 * @date 19.09.2024
 *
 * Resources:
 * - Beej's Guide to Network Programming
 *   @link https://beej.us/guide/bgnet/
 * - UNIX Network Programming (W. Richard Stevens)
 *   @link https://putregai.org/books/unix_netprog_v1.pdf
 *
 * @note Might exand with libev to handle events if many connections
 *
 ***************************************************/

int CreateSocket(const char *ip_address, int port);

// Event callback for socket read operations

#endif /* SOCKETMODULE_H */
