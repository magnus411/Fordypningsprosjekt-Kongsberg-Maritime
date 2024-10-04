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
#include <arpa/inet.h>
#include <iconv.h>

#define SDB_LOG_LEVEL 4
#include <SdbExtern.h>

#include <database_systems/Postgres.h>
#include <modules/DatabaseModule.h>
#include <common/CircularBuffer.h>

SDB_LOG_REGISTER(DatabaseModule);

ssize_t
ReadFromSensorDataBuf(circular_buffer *Cb, void *Dest, size_t ReadSize)
{
    ssize_t BytesRead = CbRead(Cb, Dest, ReadSize);
    if(BytesRead != ReadSize) {
        return BytesRead;
    }
}
