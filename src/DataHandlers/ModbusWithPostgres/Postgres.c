#include <signal.h>
#include <sys/epoll.h>

#include <src/Sdb.h>
SDB_LOG_REGISTER(PostgresThread);
SDB_THREAD_ARENAS_EXTERN(Postgres);

#include <src/Common/Time.h>
#include <src/DataHandlers/ModbusWithPostgres/ModbusWithPostgres.h>
#include <src/DatabaseSystems/DatabaseInitializer.h>
#include <src/DatabaseSystems/Postgres.h>

extern volatile sig_atomic_t GlobalShutdown;

sdb_errno
PgRun(void *Arg)
{
    sdb_errno Ret = 0;
    mbpg_ctx *Ctx = Arg;

    sdb_arena PgArena;
    u64       PgASize = Ctx->PgMemSize + PG_SCRATCH_COUNT * Ctx->PgScratchSize;
    u8       *PgAMem  = malloc(PgASize);
    SdbArenaInit(&PgArena, PgAMem, PgASize);

    PgInitThreadArenas();
    SdbThreadArenasInitExtern(Postgres);
    for(u64 s = 0; s < PG_SCRATCH_COUNT; ++s) {
        sdb_arena *Scratch = SdbArenaBootstrap(&PgArena, NULL, Ctx->PgScratchSize);
        SdbThreadArenasAdd(Scratch);
    }

    // Initialize postgres context
    postgres_ctx *PgCtx = PgPrepareCtx(&PgArena, Ctx->SdPipe);
    if(PgCtx == NULL) {
        return -1;
    }

    PGconn *Conn = PgCtx->DbConn;
    // NOTE(ingar): We unfortunately don't have time to extend the implementation to support more
    // than one table at the moment, but it should be relatively straightforward. Create more pipes
    // (one per sensor) in the Mb-Pg thread group creation function and extend the epoll waiting
    // system to have one per pipe and then write to the correct table.
    pg_table_info    *TableInfo   = PgCtx->TablesInfo[0];
    sensor_data_pipe *Pipe        = Ctx->SdPipe;
    int               ReadEventFd = Pipe->ReadEventFd;

    int EpollFd = epoll_create1(0);
    if(EpollFd == -1) {
        SdbLogError("Failed to create epoll: %s", strerror(errno));
        free(PgAMem);
        return -errno;
    }

    struct epoll_event ReadEvent
        = { .events = EPOLLIN | EPOLLERR | EPOLLHUP, .data.fd = ReadEventFd };
    if(epoll_ctl(EpollFd, EPOLL_CTL_ADD, ReadEventFd, &ReadEvent) == -1) {
        SdbLogError("Failed to start read event: %s", strerror(errno));
        free(PgAMem);
        return -errno;
    }

    u64             PgFailCounter  = 0;
    u64             TimeoutCounter = 0;
    struct timespec LoopStart, CopyStart, CopyEnd, TimeDiff;
    SdbTimeMonotonic(&LoopStart);
    static u64 TotalInsertedItems = 0;
    printf("Item count/buf: %lu\n", Pipe->ItemMaxCount);

    // NOTE(ingar): Wait for modbus (and test server if running tests) to complete its setup

    SdbBarrierWait(&Ctx->Barrier);

    while(!GlobalShutdown) {
        struct epoll_event Events[1];
        int                EpollRet = epoll_wait(EpollFd, Events, 1, SDB_TIME_MS(100));
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
                Ret = -ETIMEDOUT;
            } else {
                continue;
            }
        }

        if(Events[0].events & (EPOLLERR | EPOLLHUP)) {
            SdbLogError("Epoll error on read event fd");
            Ret = -EIO; // Use appropriate error code
            break;
        }

        if(Events[0].events & EPOLLIN) {
            sdb_arena *Buf = NULL;
            while(!GlobalShutdown && (Buf = SdPipeGetReadBuffer(Pipe, SDB_TIME_MS(100))) != NULL) {
                SdbAssert(Buf->Cur % Pipe->PacketSize == 0,
                          "Pipe does not contain a multiple of the packet size");

                u64 ItemCount = Buf->Cur / Pipe->PacketSize;
                SdbLogDebug("Inserting %lu items into db. Total is %lu", ItemCount,
                            TotalInsertedItems);
                TotalInsertedItems += ItemCount;

                SdbTimeMonotonic(&CopyStart);
                sdb_errno InsertRet
                    = PgInsertData(Conn, TableInfo, (const char *)Buf->Mem, ItemCount);
                SdbTimeMonotonic(&CopyEnd);

                SdbTimePrintSpecDiffWT(&CopyStart, &CopyEnd, &TimeDiff);
                printf("%ld.%09ld ", TimeDiff.tv_sec, TimeDiff.tv_nsec);
                SdbTimePrintSpecDiffWT(&LoopStart, &CopyEnd, &TimeDiff);
                printf("%ld.%09ld\n", TimeDiff.tv_sec, TimeDiff.tv_nsec);

                if(TotalInsertedItems >= 1e6) {
                    break;
                }

                // printf("Time for single copy: %ld.%09ld seconds\n", TimeDiff.tv_sec,
                //        TimeDiff.tv_nsec);
                // printf("Time since loop start: %ld.%09ld seconds\n", TimeDiff.tv_sec,
                //       TimeDiff.tv_nsec);

                if(InsertRet != 0) {
                    SdbLogError("Failed to insert data for the %lusthnd", ++PgFailCounter);
                    if(PgFailCounter >= 5) {
                        SdbLogError("Postgres operations have failed more than threshod. Stopping "
                                    "main loop");
                        Ret = -1;
                        break;
                    }
                } else {
                    SdbLogDebug("Pipe data inserted successfully");
                }
            }
        }
    }

    printf("Total insertion time: %ld.%09ld\n", TimeDiff.tv_sec, TimeDiff.tv_nsec);

    PQfinish(PgCtx->DbConn);
    close(EpollFd);
    free(PgAMem);

    SdbLogDebug("Exiting thread function");

    return Ret;
}
