#ifndef SOCKETMODULE_H
#define SOCKETMODULE_H

#include <sys/socket.h>
#include <sys/types.h>

int CreateSocket(const char *IpAddress, int Port);

#endif /* SOCKETMODULE_H */
