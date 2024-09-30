/*
 * Save to database
 * Read from buffers
 * Monitor database stats
 *   - Ability to prioritize data based on it
 *
 * ? Send messages to comm module ?
 * ? Metaprogramming tool to create C structs from sensor schemas ?
 */

#include <libpq-fe.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <iconv.h>

#define SDB_LOG_LEVEL 4
#include "SdbExtern.h"

#include "Postgres.h"
#include "DatabaseModule.h"

SDB_LOG_REGISTER(DatabaseModule);
