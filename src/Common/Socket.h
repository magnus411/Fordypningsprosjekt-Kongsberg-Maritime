#ifndef SOCKETMODULE_H
#define SOCKETMODULE_H

#include <src/Sdb.h>

#include <src/Common/Time.h>

SDB_BEGIN_EXTERN_C

#include <sys/socket.h>
#include <sys/types.h>

int SocketCreate(const char *IpAddress, int Port);
int SocketRecvWithTimeout(int SockFd, void *Buffer, size_t Length, sdb_timediff Timeout);

SDB_END_EXTERN_C

#endif /* SOCKETMODULE_H */
