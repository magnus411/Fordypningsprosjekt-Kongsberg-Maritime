
#include <src/DevUtils/ModbusDataParser.h>
#include <src/Sdb.h>
SDB_LOG_REGISTER(DataParser);

#include "ModbusDataParser.h"
#include <src/DatabaseSystems/DatabaseInitializer.h>
#include <src/Sdb.h>
#include <stdint.h>
#include <stdio.h>

bool
SdbDumpPipeToFile(sensor_data_pipe *pipe, const char *filename)
{
    if(!pipe || !filename) {
        SdbLogError("Invalid arguments: pipe or filename is NULL");
        return false;
    }

    FILE *dumpFile = fopen(filename, "wb");
    if(!dumpFile) {
        SdbLogError("Failed to create dump file: %s", filename);
        return false;
    }

    // Write buffer count
    u64 bufferCount = atomic_load(&pipe->FullBuffersCount);
    if(fwrite(&bufferCount, sizeof(bufferCount), 1, dumpFile) != 1) {
        SdbLogError("Failed to write buffer count to file");
        fclose(dumpFile);
        return false;
    }

    for(u64 i = 0; i < bufferCount; i++) {
        sdb_arena *buffer = pipe->Buffers[i];

        // Write buffer's used size
        u64 bufferSize = buffer->Cur;
        if(fwrite(&bufferSize, sizeof(bufferSize), 1, dumpFile) != 1) {
            SdbLogError("Failed to write buffer size to file");
            fclose(dumpFile);
            return false;
        }

        // Write buffer contents
        if(bufferSize > 0 && fwrite(buffer->Mem, 1, bufferSize, dumpFile) != bufferSize) {
            SdbLogError("Failed to write buffer content to file");
            fclose(dumpFile);
            return false;
        }
    }

    fclose(dumpFile);
    SdbLogInfo("Pipe content successfully dumped to %s", filename);
    return true;
}


bool
SdbParsePipeToCSV(const char *dumpFilename, const char *outputFilename, const char *schemaPath)
{
    if(!dumpFilename || !outputFilename || !schemaPath) {
        SdbLogError("Invalid arguments: one or more arguments are NULL");
        return false;
    }

    // Load schema
    cJSON *schema = DbInitGetConfFromFile(schemaPath, NULL);
    if(!schema) {
        SdbLogError("Failed to load schema from %s", schemaPath);
        return false;
    }

    FILE *dumpFile = fopen(dumpFilename, "rb");
    if(!dumpFile) {
        SdbLogError("Failed to open dump file: %s", dumpFilename);
        cJSON_Delete(schema);
        return false;
    }

    FILE *outputFile = fopen(outputFilename, "w");
    if(!outputFile) {
        SdbLogError("Failed to create output file: %s", outputFilename);
        fclose(dumpFile);
        cJSON_Delete(schema);
        return false;
    }

    // Read buffer count
    u64 bufferCount;
    if(fread(&bufferCount, sizeof(bufferCount), 1, dumpFile) != 1) {
        SdbLogError("Failed to read buffer count from dump file");
        fclose(dumpFile);
        fclose(outputFile);
        cJSON_Delete(schema);
        return false;
    }

    fprintf(outputFile, "Parsed Data\n==========\nTotal Buffers: %lu\n\n", bufferCount);

    // Iterate over buffers
    for(u64 i = 0; i < bufferCount; i++) {
        u64 bufferSize;
        if(fread(&bufferSize, sizeof(bufferSize), 1, dumpFile) != 1) {
            SdbLogError("Failed to read buffer size from dump file");
            break;
        }

        uint8_t *buffer = malloc(bufferSize);
        if(!buffer) {
            SdbLogError("Memory allocation failed for buffer");
            break;
        }

        if(fread(buffer, 1, bufferSize, dumpFile) != bufferSize) {
            SdbLogError("Failed to read buffer content from dump file");
            free(buffer);
            break;
        }

        // Parse buffer using schema
        fprintf(outputFile, "Buffer %lu (Size: %lu bytes)\n", i + 1, bufferSize);
        cJSON *sensorSchemas = cJSON_GetObjectItem(schema, "sensors");
        if(cJSON_IsArray(sensorSchemas)) {
            cJSON *sensorSchema = NULL;
            cJSON_ArrayForEach(sensorSchema, sensorSchemas)
            {
                cJSON *sensorName = cJSON_GetObjectItem(sensorSchema, "name");
                cJSON *sensorData = cJSON_GetObjectItem(sensorSchema, "data");

                if(sensorName && sensorData && cJSON_IsObject(sensorData)) {
                    fprintf(outputFile, "Sensor: %s\n", sensorName->valuestring);

                    size_t offset   = 0;
                    cJSON *dataAttr = NULL;
                    cJSON_ArrayForEach(dataAttr, sensorData)
                    {
                        if(offset >= bufferSize) {
                            SdbLogWarning("Buffer offset exceeds size for sensor %s",
                                          sensorName->valuestring);
                            break;
                        }

                        const char *attrType = cJSON_GetStringValue(dataAttr);
                        if(strcmp(attrType, "double precision") == 0) {
                            double value = *((double *)(buffer + offset));
                            fprintf(outputFile, "  %s: %f\n", dataAttr->string, value);
                            offset += sizeof(double);
                        } else if(strcmp(attrType, "integer") == 0) {
                            int32_t value = *((int32_t *)(buffer + offset));
                            fprintf(outputFile, "  %s: %d\n", dataAttr->string, value);
                            offset += sizeof(int32_t);
                        } else if(strcmp(attrType, "bigint") == 0) {
                            int64_t value = *((int64_t *)(buffer + offset));
                            fprintf(outputFile, "  %s: %ld\n", dataAttr->string, value);
                            offset += sizeof(int64_t);
                        }
                    }
                    fprintf(outputFile, "\n");
                }
            }
        }
        free(buffer);
    }

    fclose(dumpFile);
    fclose(outputFile);
    cJSON_Delete(schema);

    SdbLogInfo("Parsed data saved to %s", outputFilename);
    return true;
}
