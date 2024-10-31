#include <libpq-fe.h>
#include <sys/epoll.h>

#include <src/Sdb.h>

SDB_LOG_REGISTER(PostgresTest);
SDB_THREAD_ARENAS_EXTERN(Postgres);

#include "src/Common/SensorDataPipe.h"
#include "src/Common/Thread.h"
#include <src/DatabaseSystems/DatabaseInitializer.h>
#include <src/DatabaseSystems/Postgres.h>
#include <src/Libs/cJSON/cJSON.h>
#include <tests/DatabaseSystems/PostgresTest.h>
#include <tests/TestConstants.h>

sdb_errno
PgTestApiInit(database_api *Pg)
{
    PgInitThreadArenas();
    SdbThreadArenasInitExtern(Postgres);
    sdb_arena *Scratch1 = SdbArenaBootstrap(&Pg->Arena, NULL, SdbKibiByte(512));
    sdb_arena *Scratch2 = SdbArenaBootstrap(&Pg->Arena, NULL, SdbKibiByte(512));
    SdbThreadArenasAdd(Scratch1);
    SdbThreadArenasAdd(Scratch2);

    // Initialize postgres context
    Pg->Ctx = PgPrepareCtx(Pg);

    if(Pg->Ctx == NULL) {
        return -1;
    }

    return 0;
}

sdb_errno
PgTestApiRun(database_api *Pg)
{
    sdb_errno     Ret   = 0;
    postgres_ctx *PgCtx = PG_CTX(Pg);

    PGconn             *Conn          = PgCtx->DbConn;
    pg_table_info      *TableInfo     = PgCtx->TablesInfo[0];
    sdb_thread_control *ModuleControl = Pg->ModuleControl;
    sensor_data_pipe   *Pipe          = Pg->SdPipes[0];

    int ReadEventFd = Pipe->ReadEventFd;

    int EpollFd = epoll_create1(0);
    if(EpollFd == -1) {
        return -1;
    }

    struct epoll_event ReadEvent = { .events = EPOLLIN, .data.fd = ReadEventFd };
    if(epoll_ctl(EpollFd, EPOLL_CTL_ADD, ReadEventFd, &ReadEvent) == -1) {
        return -1;
    }

    struct epoll_event Events[1];
    int                LogCounter    = 0;
    u64                PgFailCounter = 0;
    while(!SdbTCtlShouldStop(ModuleControl)) {
        if(++LogCounter % 1000 == 0) {
            SdbLogDebug("Postgres test loop is still running");
        }

        // TODO(ingar): Is 1000 appropriate timeout?
        int EpollRet = epoll_wait(EpollFd, Events, 1, 1000);
        if(EpollRet == -1) {
            if(errno == EINTR) {
                continue;
            } else {
                Ret = -errno;
                break;
            }
        } else if(EpollRet == 0) {
            continue;
        } else {
            sdb_errno InsertRet = PgInsertData(Conn, TableInfo, Pipe);
            if(InsertRet != 0) {
                SdbLogError("Failed to insert data for the %lusth", ++PgFailCounter);
                if(PgFailCounter > 5) {
                    SdbLogError(
                        "Postgres operations have failed more than threshod. Stopping main loop");
                    break;
                }
            }
        }
    }

    close(EpollFd);
    SdbLogDebug("Exiting postgres test main loop");

    return Ret;
}

sdb_errno
PgTestApiFinalize(database_api *Pg)
{
    postgres_ctx *PgCtx = PG_CTX(Pg);
    PQfinish(PgCtx->DbConn);
    return 0;
}
