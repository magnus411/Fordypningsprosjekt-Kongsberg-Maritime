#include <stdio.h>
#include <stdlib.h>
#include "cjson/cJSON.h"
#include <libpq-fe.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

#define MAX_BUFFER_SIZE 1024
char *
get_name_from_json(cJSON *Json)
{
    cJSON *Name = cJSON_GetObjectItemCaseSensitive(Json, "name");
    if(cJSON_IsString(Name) && (Name->valuestring != NULL)) {
        return strdup(Name->valuestring);
    }
    return NULL;
}

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
bool
check_if_table_exists(PGconn *Conn, const char *Name)
{

    const char *QueryTemplate
        = "SELECT EXISTS (SELECT 1 FROM pg_tables WHERE schemaname = 'public' AND tablename = $1)";

    const char *Params[1];
    Params[0] = Name;

    PGresult *Res = PQexecParams(Conn, QueryTemplate, 1, NULL, Params, NULL, NULL, 0);

    if(PQresultStatus(Res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Query failed: %s", PQerrorMessage(Conn));
        PQclear(Res);
        return false;
    }

    bool Exists = PQgetvalue(Res, 0, 0)[0] == 't';
    PQclear(Res);
    return Exists;
}

char *
CreateTableCreationQuery(PGconn *Conn, cJSON *Json)
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

void
all_together_now(PGconn *Conn)
{
    char **SchemaNames = DbInitGetSchemasFromDb(Conn);

    if(SchemaNames == NULL) {

        fprintf(stderr, "%s\n");
        return;
    }

    for(int i = 0; SchemaNames[i] != NULL; ++i) {

        printf("Table '%s' does not exist. Creating...\n", SchemaNames[i]);

        cJSON    *SchemaJson  = cJSON_Parse(SchemaNames[i]);
        char     *CreateQuery = CreateTableCreationQuery(Conn, SchemaJson);
        PGresult *CreateRes   = PQexec(Conn, CreateQuery);

        if(PQresultStatus(CreateRes) != PGRES_COMMAND_OK) {
            fprintf(stderr, "Table creation failed: %s\n", PQerrorMessage(Conn));
        } else {
            printf("Table '%s' created successfully.\n", SchemaNames[i]);
        }
        PQclear(CreateRes);
        free(CreateQuery);
        cJSON_Delete(SchemaJson);

        free(SchemaNames[i]);
    }
    free(SchemaNames);
}
