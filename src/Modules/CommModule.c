
#include <src/Sdb.h>
SDB_LOG_REGISTER(CommModule);

#include <src/CommProtocols/CommProtocols.h>
#include <src/Common/Thread.h>
#include <src/Modules/CommModule.h>

#define CP_INIT_ATTEMPT_THRESHOLD (5)

sdb_errno
CommModuleRun(sdb_thread *Thread)
{
    comm_module_ctx  *CommCtx = Thread->Args;
    sdb_errno         Ret     = 0;
    comm_protocol_api ThreadCp;

    if((Ret = CommCtx->InitApi(CommCtx->CpType, &CommCtx->SdPipe, &CommCtx->Arena,
                               CommCtx->CpArenaSize, Thread->pid, &ThreadCp))
       == -SDBE_CP_UNAVAIL) {
        SdbLogWarning("Thread %ld: Attempting to use %s, but its API is unavailable", Thread->pid,
                      CpTypeToName(CommCtx->CpType));
        goto exit;
    }


    i64 Attempts = 0;
    while(((Ret = ThreadCp.Init(&ThreadCp)) != 0) && Attempts++ < CP_INIT_ATTEMPT_THRESHOLD) {
        SdbLogError("Thread %ld: Error on comm protocol init attempt %ld, ret: %d", Thread->pid,
                    Attempts, Ret);
    }

    SdbBarrierWait(CommCtx->ModulesBarrier);

    if(Attempts >= CP_INIT_ATTEMPT_THRESHOLD) {
        SdbLogError("Thread %ld: Comm protocol init attempt threshold exceeded", Thread->pid);
        goto exit;
    } else {
        SdbLogInfo("Thread %ld: Comm protocol successfully initialized", Thread->pid);
    }


    if((Ret = ThreadCp.Run(&ThreadCp)) >= 0) {
        SdbLogInfo("Thread %ld: Comm protocol has stopped and returned success", Thread->pid);
    } else {
        SdbLogError("Thread %ld: Comm protocol has stopped and returned an error: %d", Thread->pid,
                    Ret);
    }


    if((Ret = ThreadCp.Finalize(&ThreadCp)) == 0) {
        SdbLogInfo("Thread %ld: Comm protocol successfully finalized", Thread->pid);
    } else {
        SdbLogError("Thread %ld: Comm protocol finalization failed", Thread->pid);
    }

exit:
    return Ret;
}
