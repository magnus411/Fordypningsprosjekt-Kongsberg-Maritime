#ifndef SOCKETMODULE_H
#define SOCKETMODULE_H

#include <src/Sdb.h>

SDB_BEGIN_EXTERN_C

#include <sys/socket.h>
#include <sys/types.h>

int CreateSocket(const char *IpAddress, int Port);

SDB_END_EXTERN_C

#endif /* SOCKETMODULE_H */
