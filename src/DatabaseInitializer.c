#include <stdio.h>
#include <stdlib.h>
#include "cjson/cJSON.h"
#include <libpq-fe.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#define SDB_LOG_LEVEL 4
#include <SdbExtern.h>

SDB_LOG_REGISTER(InitDb);

#define MAX_BUFFER_SIZE 1024
#define MAX_FILE_SIZE   1024
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

cJSON *
DbInitGetSchemaFromFile(const char *Filename)
{
    FILE *Fp = fopen(Filename, "rb");
    if(Fp == NULL) {
        fprintf(stderr, "Could not open file %s\n", Filename);
        return NULL;
    }

    fseek(Fp, 0L, SEEK_END);
    long FileSize = ftell(Fp);
    if(FileSize > MAX_FILE_SIZE) {
        fprintf(stderr, "File size exceeds maximum allowed size of 1MB\n");
        fclose(Fp);
        return NULL;
    }
    fseek(Fp, 0L, SEEK_SET);

    char *Buffer = malloc(FileSize + 1);
    if(Buffer == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        fclose(Fp);
        return NULL;
    }

    size_t readSize  = fread(Buffer, 1, FileSize, Fp);
    Buffer[readSize] = '\0';

    fclose(Fp);

    cJSON *Schema = cJSON_Parse(Buffer);
    if(Schema == NULL) {
        fprintf(stderr, "Error parsing JSON: %s\n", cJSON_GetErrorPtr());
        free(Buffer);
        return NULL;
    }

    free(Buffer);

    return Schema;
}

char *
DbInitCreateTableCreationQuery(PGconn *Conn, cJSON *Json)
{
    if(Json == NULL || !cJSON_IsObject(Json)) {
        printf("Error: Input was not a valid, parsed JSON object.\n");
        exit(1);
    }

    cJSON *TableNameItem = cJSON_GetObjectItem(Json, "name");
    if(TableNameItem == NULL || !cJSON_IsString(TableNameItem)) {
        printf("ERROR: JSON does not contain a \"name\" field\n");
        exit(1);
    }
    const char *Name = TableNameItem->valuestring;

    char *Query = malloc(MAX_BUFFER_SIZE); // Fixed large buffer

    if(Query == NULL) {
        printf("Memory allocation failed\n");
        return NULL;
    }

    snprintf(Query, MAX_BUFFER_SIZE, "CREATE TABLE IF NOT EXISTS %s (\n", Name);
    strcat(Query, "id SERIAL PRIMARY KEY,\n");
    strcat(Query, "protocol TEXT,\n");

    cJSON *DataItem = cJSON_GetObjectItem(Json, "data");
    if(DataItem == NULL || !cJSON_IsObject(DataItem)) {
        printf("ERROR: JSON does not contain a \"data\" object\n");
        free(Query);
        return NULL;
    }

    cJSON *CurrentElement = NULL;
    int    FieldCount     = 0;

    cJSON_ArrayForEach(CurrentElement, DataItem)
    {
        if(cJSON_IsString(CurrentElement)) {
            const char *Type = CurrentElement->valuestring;
            char        Field[256];

            // Append each field to the query
            snprintf(Field, sizeof(Field), "%s %s,\n", CurrentElement->string, Type);
            if(strlen(Query) + strlen(Field) < MAX_BUFFER_SIZE) {
                strcat(Query, Field);
            } else {
                printf("Error: Query exceeds buffer size\n");
                free(Query);
                return NULL;
            }

            FieldCount++;
        }
    }

    if(FieldCount > 0) {
        Query[strlen(Query) - 2] = '\0';
    }

    strcat(Query, "\n);");
    return Query;
}
