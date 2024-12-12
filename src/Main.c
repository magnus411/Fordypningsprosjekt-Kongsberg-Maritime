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

#include <src/DevUtils/ModbusDataParser.h>
#include <src/Signals.h>

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

    // NOTE(ingar): Has to be done this way because of the goto statements
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


int
main(int ArgCount, char **ArgV)
{
    ConvertDumpToCSV("./dumps/pipe_dump_20241211_211549.bin", "./dumps/sensor_data.csv");
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

    SdbSetMemoryDumpPath("/memory_dump.log");


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
