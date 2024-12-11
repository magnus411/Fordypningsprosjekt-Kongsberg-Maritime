#include <libpq-fe.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <src/Sdb.h>

SDB_LOG_REGISTER(DatabaseInitializer);

#include <src/Libs/cJSON/cJSON.h>

cJSON *
DbInitGetConfFromFile(const char *Filename, sdb_arena *A)
{
    sdb_scratch_arena Scratch;
    if(A) {
        Scratch = SdbScratchBegin(A);
    } else {
        Scratch.Arena = NULL;
    }

    sdb_file_data *SchemaFile = SdbLoadFileIntoMemory(Filename, Scratch.Arena);
    if(SchemaFile == NULL) {
        SdbLogError("Failed to load config file into memory");
        return NULL;
    }

    cJSON *Schema = cJSON_Parse((char *)SchemaFile->Data);
    if(Schema == NULL) {
        SdbLogError("Error parsing JSON: %s", cJSON_GetErrorPtr());
    }

    if(A != NULL) {
        SdbScratchRelease(Scratch);
    } else {
        free(SchemaFile);
    }

    return Schema;
}
