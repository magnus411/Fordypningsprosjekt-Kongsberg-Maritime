#include <sys/epoll.h>

#include <src/Sdb.h>
SDB_LOG_REGISTER(PostgresApi);
SDB_THREAD_ARENAS_EXTERN(Postgres);

#include <src/DatabaseSystems/DatabaseInitializer.h>
#include <src/DatabaseSystems/DatabaseSystems.h>
#include <src/DatabaseSystems/Postgres.h>

sdb_errno
PgInit(database_api *Pg)
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
PgRun(database_api *Pg)
{
    sdb_errno     Ret   = 0;
    postgres_ctx *PgCtx = PG_CTX(Pg);

    PGconn             *Conn      = PgCtx->DbConn;
    pg_table_info      *TableInfo = PgCtx->TablesInfo[0]; // TODO(ingar): Generalize to more tables
    sdb_thread_control *ModuleControl = Pg->ModuleControl;
    sensor_data_pipe   *Pipe          = Pg->SdPipes[0];
    int                 ReadEventFd   = Pipe->ReadEventFd;

    int EpollFd = epoll_create1(0);
    if(EpollFd == -1) {
        SdbLogError("Failed to create epoll: %s", strerror(errno));
        return -errno;
    }

    struct epoll_event ReadEvent = { .events = EPOLLIN, .data.fd = ReadEventFd };
    if(epoll_ctl(EpollFd, EPOLL_CTL_ADD, ReadEventFd, &ReadEvent) == -1) {
        SdbLogError("Failed to start read event: %s", strerror(errno));
        return -errno;
    }

    int LogCounter     = 0;
    u64 PgFailCounter  = 0;
    u64 TimeoutCounter = 0;
    while(!SdbTCtlShouldStop(ModuleControl)) {
        if(++LogCounter % 100 == 0) {
            SdbLogDebug("Postgres test loop is still running");
        }

        // TODO(ingar): Is 1000 appropriate timeout?
        struct epoll_event Events[1];
        int                EpollRet = epoll_wait(EpollFd, Events, 1, SDB_TIME_S(1));
        if(EpollRet == -1) {
            if(errno == EINTR) {
                SdbLogWarning("Epoll wait received interrupt");
                continue;
            } else {
                SdbLogError("Epoll wait failed: %s", strerror(errno));
                Ret = -errno;
                break;
            }
        } else if(EpollRet == 0) {
            SdbLogInfo("Epoll wait timed out for the %lusthnd time", ++TimeoutCounter);
            if(TimeoutCounter >= 5) {
                SdbLogError("Epoll has timed out more than threshold. Stopping main loop");
                Ret = -1;
            } else {
                continue;
            }
        } else {
            sdb_errno InsertRet = PgInsertData(Conn, TableInfo, Pipe);
            if(InsertRet != 0) {
                SdbLogError("Failed to insert data for the %lusthnd", ++PgFailCounter);
                if(PgFailCounter >= 5) {
                    SdbLogError(
                        "Postgres operations have failed more than threshod. Stopping main loop");
                    Ret = -1;
                    break;
                }
            } else {
                SdbLogDebug("Pipe data inserted successfully");
            }
        }
    }

    close(EpollFd);
    SdbLogDebug("Exiting postgres main loop");

    return Ret;
}

sdb_errno
PgFinalize(database_api *Pg)
{
    postgres_ctx *PgCtx = PG_CTX(Pg);
    PQfinish(PgCtx->DbConn);
    return 0;
}
