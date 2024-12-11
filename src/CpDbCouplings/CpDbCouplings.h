#ifndef CP_DB_COUPLINGS
#define CP_DB_COUPLINGS

#include <src/Sdb.h>

#include <src/Common/ThreadGroup.h>
#include <src/Libs/cJSON/cJSON.h>

tg_group *CdcCreateTg(cJSON *Conf, u64 GroupId, sdb_arena *A);
void      CdcGetMemAndScratchSize(cJSON *Conf, u64 *MemSize, u64 *ScratchSize);

#endif
