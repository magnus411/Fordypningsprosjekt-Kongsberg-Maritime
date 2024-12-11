#include <src/Sdb.h>
SDB_LOG_REGISTER(CpDbCouplings);

#include <src/Common/ThreadGroup.h>
#include <src/Libs/cJSON/cJSON.h>

#include <src/CpDbCouplings/ModbusWithPostgres/ModbusWithPostgres.h>

enum coupling_id
{
    Mb_With_Postgres = 0,
};

static const char *AvailableCouplingNames[] = { "modbus_with_postgres" };

static enum coupling_id
GetCouplingIdFromName(const char *Name)
{
    u64 ArrLen = SdbArrayLen(AvailableCouplingNames);
    for(i32 i = 0; i < ArrLen; ++i) {
        if(strcmp(Name, AvailableCouplingNames[i]) == 0) {
            return i;
        }
    }

    return -1;
}

void
CdcGetMemAndScratchSize(cJSON *Conf, u64 *MemSize, u64 *ScratchSize)
{
    cJSON *MemSizeObj     = cJSON_GetObjectItem(Conf, "mem");
    cJSON *ScratchSizeObj = cJSON_GetObjectItem(Conf, "scratch_size");
    if(!cJSON_IsString(MemSizeObj) || !cJSON_IsString(ScratchSizeObj)) {
        SdbAssert(0, "mem and/or scratch_size were not present or were malformed in sdb_conf.json");
        return;
    }

    *MemSize     = SdbMemSizeFromString(cJSON_GetStringValue(MemSizeObj));
    *ScratchSize = SdbMemSizeFromString(cJSON_GetStringValue(ScratchSizeObj));
}

tg_group *
CdcCreateTg(cJSON *Conf, u64 GroupId, sdb_arena *A)
{
    cJSON *Enabled = cJSON_GetObjectItem(Conf, "enabled");
    if(!(cJSON_IsBool(Enabled) && cJSON_IsTrue(Enabled))) {
        return NULL;
    }

    cJSON      *CouplingNameObj = cJSON_GetObjectItem(Conf, "name");
    const char *CouplingName    = cJSON_GetStringValue(CouplingNameObj);

    tg_group        *Group;
    enum coupling_id CouplingId = GetCouplingIdFromName(CouplingName);
    switch(CouplingId) {
        case Mb_With_Postgres:
            {
                Group = MbPgCreateTg(Conf, GroupId, A);
                if(Group != NULL) {
                    SdbLogInfo("Created thread group for Modbus with Postgres");
                } else {
                    SdbLogError("Failed to create thread group for Modbus with Postgres");
                }
            }
            break;
        default:
            Group = NULL;
    }

    return Group;
}
