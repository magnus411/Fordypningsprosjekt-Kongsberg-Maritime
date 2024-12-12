#ifndef SDB_SIGNALS_H
#define SDB_SIGNALS_H

#include <signal.h>
#include <src/Common/SensorDataPipe.h>
#include <src/Libs/cJSON/cJSON.h>
#include <stdbool.h>

typedef struct signal_handler_context
{
    struct tg_manager    *manager;
    char                 *memory_dump_path;
    volatile sig_atomic_t shutdown_flag;
    sensor_data_pipe     *pipe;
} signal_handler_context;

extern signal_handler_context g_signal_context;

struct tg_manager;

int  SdbSetupSignalHandlers(struct tg_manager *manager);
void SdbSetMemoryDumpPath(const char *path);

bool SdbShouldShutdown(void);
bool SdbDumpSensorDataPipe(sensor_data_pipe *pipe, const char *filename);


void ConvertDumpToCSV(const char *dumpFileName, const char *csvFileName);

#endif // SDB_SIGNALS_H
