/**
 * @file DataHandlers.c
 * @brief Implementation of Data Handler Thread Group Management
 *
 * Provides the core implementation for creating and managing
 * thread groups based on configuration. Handles dynamic
 * handler selection and initialization process.
 *
 */


#include <src/Sdb.h>
SDB_LOG_REGISTER(DataHandlers);

#include <src/Common/ThreadGroup.h>
#include <src/Libs/cJSON/cJSON.h>

#include <src/DataHandlers/ModbusWithPostgres/ModbusWithPostgres.h>

enum handler_id
{
    Mb_With_Postgres = 0,
};

/**
 * @brief Array of available handler names
 *
 * Contains string representations of supported data handlers.
 * Must be kept in sync with handler_id enumeration.
 */
static const char *AvailableHandlers[] = { "modbus_with_postgres" };


/**
 * @brief Converts handler name to its corresponding ID
 *
 * Searches through AvailableHandlers to find the matching
 * handler ID for a given name.
 *
 * @param Name Name of the handler to look up
 * @return enum handler_id Numeric ID of the handler, or -1 if not found
 */
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


/**
 * @brief Extracts memory and scratch sizes from JSON configuration
 *
 * Retrieves memory and scratch sizes from the provided JSON
 * configuration. Performs validation and converts string
 * representations to numeric sizes.
 *
 * @param Conf JSON configuration object
 * @param MemSize Pointer to store the extracted memory size
 * @param ScratchSize Pointer to store the extracted scratch size
 *
 * @note Asserts if configuration is invalid or missing required fields
 */
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


/**
 * @brief Creates a thread group based on configuration
 *
 * Determines the appropriate handler based on the configuration
 * and creates a corresponding thread group. Currently supports
 * Modbus with Postgres handler.
 *
 * @param Conf JSON configuration object
 * @param GroupId Unique identifier for the thread group
 * @param A Memory arena for allocation
 *
 * @return tg_group* Pointer to the created thread group, or NULL if creation fails
 *
 * @note Logs information or error messages during thread group creation
 */
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
