#ifndef DB_CONNECTION_H
#define DB_CONNECTION_H

#include <libpq-fe.h>

PGconn* connect_db(const char* conninfo);
void disconnect_db(PGconn *conn);

#endif

