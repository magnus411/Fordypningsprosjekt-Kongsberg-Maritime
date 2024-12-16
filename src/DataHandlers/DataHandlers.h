/**
 * @file DataHandlers.h
 * @brief Data Handler Management for Thread Group Creation
 *
 * This header provides interfaces for creating and configuring
 * data handler thread groups based on JSON configuration. It
 * supports dynamic selection and initialization of different
 * data handling mechanisms.
 */


#ifndef CP_DB_COUPLINGS
#define CP_DB_COUPLINGS

#include <src/Sdb.h>

#include <src/Common/ThreadGroup.h>
#include <src/Libs/cJSON/cJSON.h>


/**
 * @brief Creates a thread group based on the configuration
 *
 * This function creates a thread group for a specific data handler
 * based on the provided configuration. Currently supports Modbus
 * with Postgres handler.
 *
 * @param Conf Pointer to the JSON configuration object
 * @param GroupId Unique identifier for the thread group
 * @param A Memory arena for allocation
 *
 * @return tg_group* Pointer to the created thread group, or NULL if creation fails
 */
tg_group *DhsCreateTg(cJSON *Conf, u64 GroupId, sdb_arena *A);


/**
 * @brief Extracts memory and scratch size from configuration
 *
 * Retrieves memory and scratch sizes from the JSON configuration.
 * Asserts if the configuration is malformed or missing required fields.
 *
 * @param Conf Pointer to the JSON configuration object
 * @param MemSize Pointer to store the memory size
 * @param ScratchSize Pointer to store the scratch size
 */
void DhsGetMemAndScratchSize(cJSON *Conf, u64 *MemSize, u64 *ScratchSize);

#endif
