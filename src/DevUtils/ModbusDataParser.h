#ifndef MODBUS_DATA_PARSER_H
#define MODBUS_DATA_PARSER_H

#include <src/Common/SensorDataPipe.h>
#include <src/Libs/cJSON/cJSON.h>

bool SdbDumpPipeToFile(sensor_data_pipe *pipe, const char *filename);
bool SdbParsePipeToCSV(const char *dumpFilename, const char *outputFilename,
                       const char *schemaPath);
#endif // MODBUS_DATA_PARSER_H
