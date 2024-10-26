#include <libpq-fe.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <src/Sdb.h>

SDB_LOG_REGISTER(DatabaseInitializer);

#include <src/Libs/cJSON/cJSON.h>

#define MAX_BUFFER_SIZE 1024
#define MAX_FILE_SIZE   1024

// NOTE(ingar): Since this is Postgres specific, it doesn't really belong here, but I'll just keep
// it commented out here for future reference
/*
cJSON **
DbInitGetSchemasFromDb(PGconn *Conn)
{
    PGresult *Res = PQexec(Conn, "SELECT data FROM schemas");

    if(PQresultStatus(Res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "SELECT failed: %s", PQerrorMessage(Conn));
        PQclear(Res);
        PQfinish(Conn);
        return NULL;
    }

    int     RowCount    = PQntuples(Res);
    cJSON **SchemaArray = malloc((RowCount + 1) * sizeof(cJSON *));
    if(SchemaArray == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        PQclear(Res);
        PQfinish(Conn);
        return NULL;
    }

    for(int i = 0; i < RowCount; i++) {
        const char *SchemaJsonString = PQgetvalue(Res, i, 0);
        cJSON      *SchemaJson       = cJSON_Parse(SchemaJsonString);

        if(SchemaJson == NULL) {
            fprintf(stderr, "Error parsing JSON: %s\n", cJSON_GetErrorPtr());
            for(int j = 0; j < i; j++) {
                cJSON_Delete(SchemaArray[j]);
            }
            free(SchemaArray);
            PQclear(Res);
            return NULL;
        }

        SchemaArray[i] = SchemaJson;
    }

    SchemaArray[RowCount] = NULL;
    PQclear(Res);
    return SchemaArray;
}
*/

cJSON *
DbInitGetConfFromFile(const char *Filename, sdb_arena *Arena)
{
    sdb_file_data *SchemaFile = SdbLoadFileIntoMemory(Filename, Arena);
    cJSON         *Schema     = cJSON_Parse((char *)SchemaFile->Data);
    if(Schema == NULL) {
        SdbLogError("Error parsing JSON: %s", cJSON_GetErrorPtr());
    }

    return Schema;
}
