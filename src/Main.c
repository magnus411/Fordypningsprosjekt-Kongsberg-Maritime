/**
 * @file Main.c
 * @brief Main implementation file for the Sensor Data Handler System
 * @author Magnus Gjerstad, Ingar Asheim, Ole Fredrik Høivang Heggum
 * @date 2024
 *
 * This file implements the main entry point and configuration setup for the
 * Kongsberg Maritime's sensor data handling system. The system is designed to
 * collect and store sensor data with flexible and modular configuration capabilities.
 *
 * This system can be configured to handle multiple data handlers, each with their
 * own thread group. The system is designed to be easily extensible and configurable
 * through JSON configuration files.
 *
 * Changes to the code can be made to extend the system's capabilities, such as adding protocols.
 * This system has buildt up a framework for adding new configurations later.
 */

#include <signal.h>
#include <stdlib.h>
#include <string.h>

#define SDB_H_IMPLEMENTATION
#include <src/Sdb.h>
#undef SDB_H_IMPLEMENTATION

SDB_LOG_REGISTER(Main);

#include <src/Common/Thread.h>
#include <src/Common/ThreadGroup.h>
#include <src/DataHandlers/DataHandlers.h>
#include <src/Libs/cJSON/cJSON.h>
#include <src/Signals.h>

/**
 * @brief Sets up the thread group manager from a configuration file
 *
 * This function reads and parses a JSON configuration file to set up data handlers
 * and their corresponding thread groups. It creates a thread group manager that
 * oversees all configured data handlers.
 *
 *
 *
 * @param[in] ConfFilename Path to the JSON configuration file
 * @param[out] Manager Pointer to the thread group manager pointer that will be initialized
 *
 * @return sdb_errno Returns 0 on success, or one of the following error codes:
 *         - -ENOMEM: Memory allocation failed
 *         - -SDBE_JSON_ERR: JSON parsing error
 *         - -EINVAL: No data handlers found in configuration
 *         - -1: General failure in thread group creation or manager initialization
 *
 * @note The caller is responsible for destroying the manager using TgDestroyManager()
 *       when it's no longer needed
 */
sdb_errno
SetUpFromConf(sdb_string ConfFilename, tg_manager **Manager)
{
    sdb_errno      Ret               = 0;
    sdb_file_data *ConfFile          = NULL;
    cJSON         *Conf              = NULL;
    cJSON         *DataHandlersConfs = NULL;
    u64            HandlerCount      = 0;
    u64            tg                = 0;
    cJSON         *HandlerConf       = NULL;
    tg_group     **Tgs               = NULL;

    ConfFile = SdbLoadFileIntoMemory(ConfFilename, NULL);
    if(ConfFile == NULL) {
        return -ENOMEM;
    }

    Conf = cJSON_Parse((char *)ConfFile->Data);
    if(Conf == NULL) {
        Ret = -SDBE_JSON_ERR;
        goto cleanup;
    }

    DataHandlersConfs = cJSON_GetObjectItem(Conf, "data_handlers");
    if(DataHandlersConfs == NULL || !cJSON_IsArray(DataHandlersConfs)) {
        Ret = -SDBE_JSON_ERR;
        goto cleanup;
    }

    HandlerCount = cJSON_GetArraySize(DataHandlersConfs);
    if(HandlerCount <= 0) {
        SdbLogError("No data handlers were found in the configuration file");
        Ret = -EINVAL;
        goto cleanup;
    }

    Tgs = malloc(sizeof(tg_group *) * HandlerCount);
    if(!Tgs) {
        Ret = -ENOMEM;
        goto cleanup;
    }

    cJSON_ArrayForEach(HandlerConf, DataHandlersConfs)
    {
        Tgs[tg] = DhsCreateTg(HandlerConf, tg, NULL);
        if(Tgs[tg] == NULL) {
            cJSON *CouplingName = cJSON_GetObjectItem(HandlerConf, "name");
            SdbLogError("Unable to create thread group for data handler %s",
                        cJSON_GetStringValue(CouplingName));
            Ret = -1;
            goto cleanup;
        }
        ++tg;
    }

    *Manager = TgCreateManager(Tgs, HandlerCount, NULL);
    if(!*Manager) {
        SdbLogError("Failed to create TG manager");
        Ret = -1;
    }

cleanup:
    if(Ret == -SDBE_JSON_ERR) {
        SdbLogError("Error parsing JSON: %s", cJSON_GetErrorPtr());
    }
    if((Ret != 0) && *Manager) {
        TgDestroyManager(*Manager);
    }
    if(Tgs) {
        free(Tgs);
    }
    free(ConfFile);
    cJSON_Delete(Conf);
    return Ret;
}

/**
 * @brief Main entry point for the Sensor Data Handler System
 *
 * This function initializes and runs the sensor data handling system. It performs
 * the following main tasks:
 * - Sets up the system configuration from a JSON file
 * - Initializes signal handlers
 * - Starts all thread groups
 * - Waits for completion and performs cleanup
 *
 * @param ArgCount Number of command line arguments
 * @param ArgV Array of command line argument strings
 *
 * @return EXIT_SUCCESS on successful execution, EXIT_FAILURE if initialization fails
 */
int
main(int ArgCount, char **ArgV)
{
    tg_manager *Manager = NULL;
    SetUpFromConf("./configs/sdb_conf.json", &Manager);
    if(Manager == NULL) {
        SdbLogError("Failed to set up from config file");
        exit(EXIT_FAILURE);
    } else {
        SdbLogInfo("Successfully set up from config file!");
    }


    if(SdbSetupSignalHandlers(Manager) != 0) {
        SdbLogError("Failed to setup signal handlers");
        TgDestroyManager(Manager);
        exit(EXIT_FAILURE);
    }


    SdbLogInfo("Starting all thread groups");
    sdb_errno TgStartRet = TgManagerStartAll(Manager);
    if(TgStartRet != 0) {
        SdbLogError("Failed to start thread groups. Exiting");
        exit(EXIT_FAILURE);
    } else {
        SdbLogInfo("Successfully started all thread groups!");
    }


    TgManagerWaitForAll(Manager);
    TgDestroyManager(Manager);

    return EXIT_SUCCESS;
}
