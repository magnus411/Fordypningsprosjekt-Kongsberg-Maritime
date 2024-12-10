#ifndef CP_DB_COUPLINGS
#define CP_DB_COUPLINGS

#include <src/Sdb.h>

#include <src/Common/ThreadGroup.h>
#include <src/Libs/cJSON/cJSON.h>

tg_group *CpDbCouplingCreateTg(cJSON *Conf, tg_manager *Manager, i32 GroupId, sdb_arena *A);

#endif
