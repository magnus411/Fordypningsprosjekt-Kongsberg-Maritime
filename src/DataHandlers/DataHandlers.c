#include <src/Sdb.h>
SDB_LOG_REGISTER(CpDbCouplings);

#include <src/Common/ThreadGroup.h>
#include <src/Libs/cJSON/cJSON.h>

#include <src/DataHandlers/ModbusWithPostgres/ModbusWithPostgres.h>

enum handler_id
{
    Mb_With_Postgres = 0,
};

static const char *AvailableHandlers[] = { "modbus_with_postgres" };

static enum handler_id
GetHandlerIdFromName(const char *Name)
{
    u64 ArrLen = SdbArrayLen(AvailableHandlers);
    for(i32 i = 0; i < ArrLen; ++i) {
        if(strcmp(Name, AvailableHandlers[i]) == 0) {
            return i;
        }
    }

    return -1;
}

void
DhsGetMemAndScratchSize(cJSON *Conf, u64 *MemSize, u64 *ScratchSize)
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
DhsCreateTg(cJSON *Conf, u64 GroupId, sdb_arena *A)
{
    cJSON *Enabled = cJSON_GetObjectItem(Conf, "enabled");
    if(!(cJSON_IsBool(Enabled) && cJSON_IsTrue(Enabled))) {
        return NULL;
    }

    cJSON      *HandlerNameObj = cJSON_GetObjectItem(Conf, "name");
    const char *HandlerName    = cJSON_GetStringValue(HandlerNameObj);

    tg_group       *Group;
    enum handler_id HandlerId = GetHandlerIdFromName(HandlerName);
    switch(HandlerId) {
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
