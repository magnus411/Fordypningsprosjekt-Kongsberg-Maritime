
#include <src/Sdb.h>
SDB_LOG_REGISTER(CommModule);

#include <src/CommProtocols/CommProtocols.h>
#include <src/Common/Errno.h>
#include <src/Modules/CommModule.h>

#define CP_INIT_ATTEMPT_THRESHOLD (5)

void *
CommModuleRun(void *CommCtx_)
{
    comm_module_ctx  *CommCtx = CommCtx_;
    sdb_errno         Ret     = 0;
    void             *OptArgs = NULL;
    comm_protocol_api ThreadCp;

    if((Ret = CpInitApi(CommCtx->CpType, &CommCtx->SdPipe, &CommCtx->Arena, CommCtx->CpArenaSize,
                        CommCtx->ThreadId, &ThreadCp, OptArgs))
       == -SDBE_CP_UNAVAIL) {
        SdbLogWarning("Thread %ld: Attempting to use %s, but its API is unavailable",
                      CommCtx->ThreadId, CpTypeToName(CommCtx->CpType));
        goto exit;
    }

    i64 Attempts = 0;
    while(((Ret = ThreadCp.Init(&ThreadCp, OptArgs)) != 0)
          && Attempts++ < CP_INIT_ATTEMPT_THRESHOLD) {
        SdbLogError("Thread %ld: Error on comm protocol init attempt %ld, ret: %d",
                    CommCtx->ThreadId, Attempts, Ret);
    }

    if(Attempts >= CP_INIT_ATTEMPT_THRESHOLD) {
        SdbLogError("Thread %ld: Comm protocol init attempt threshold exceeded", CommCtx->ThreadId);
        goto exit;
    } else {
        SdbLogInfo("Thread %ld: Comm protocol successfully initialized", CommCtx->ThreadId);
    }

    if((Ret = ThreadCp.Run(&ThreadCp)) == 0) {
        SdbLogInfo("Thread %ld: Comm protocol has stopped and returned success", CommCtx->ThreadId);
    } else {
        SdbLogError("Thread %ld: Comm protocol has stopped and returned an error: %d",
                    CommCtx->ThreadId, Ret);
        goto exit;
    }

    if((Ret = ThreadCp.Finalize(&ThreadCp)) == 0) {
        SdbLogInfo("Thread %ld: Comm protocol successfully finalized", CommCtx->ThreadId);
    } else {
        SdbLogError("Thread %ld: Comm protocol finalization failed", CommCtx->ThreadId);
    }

exit:
    CommCtx->Errno = Ret;
    return NULL;
}
