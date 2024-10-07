#include <stdio.h>
#include <stdlib.h>
#include "cjson/cJSON.h"
#include <libpq-fe.h>
#include <stdbool.h>
#include <string.h>

// Get all sensor schemas from db, grab all schema names from json.
// Check all schema names against all existing tables.
// If the table exists, do nothing.
// If it does not, get all fields of schema and create a table with it.

char *
get_name_from_json(const char *JsonString)
{
    cJSON *Json = cJSON_Parse(JsonString);
    cJSON *Name = cJSON_GetObjectItemCaseSensitive(Json, "name");
    if(cJSON_IsString(Name) && (Name->valuestring != NULL))
    {
        cJSON_free(Json);
        cJSON_free(Name);
        return strdup(Name->valuestring);
    }
    cJSON_free(Json);
    cJSON_free(Name);
    return NULL;
}

char **
get_all_schema_names_from_db(PGconn *Conn)
{
    PGresult *Res = PQexec(Conn, "SELECT * FROM schemas");

    if(PQresultStatus(Res) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "SELECT failed: %s", PQerrorMessage(Conn));
        PQclear(Res);
        PQfinish(Conn);
        exit(1);
    }

    int    RowCount  = PQntuples(Res);
    char **NameArray = malloc(RowCount * sizeof(char *));

    for(int i = 0; i < RowCount; i++)
    {
        NameArray[i] = strdup(get_name_from_json(PQgetvalue(Res, i, 1)));
    }

    return NameArray;
}

bool
check_if_table_exists(PGconn *Conn, const char *Name)
{
    char *QueryTemplate
        = "SELECT EXISTS (SELECT 1 FROM pg_tables WHERE schemaname = 'public' AND tablename = '%s'";
    size_t QueryLength = snprintf(NULL, 0, QueryTemplate, Name);
    char  *Query       = malloc(QueryLength + 1);

    if(Query == NULL)
    {
        fprintf(stderr, "Memory allocation failed");
        return false;
    }

    snprintf(Query, QueryLength + 1, QueryTemplate, Name);

    PGresult *Res = PQexec(Conn, Query);

    if(PQresultStatus(Res) != PGRES_TUPLES_OK)
    {
        fprintf(stderr, "Query failed: %s", PQerrorMessage(Conn));
        PQclear(Res);
        return false;
    }

    bool Exists = PQgetvalue(Res, 0, 0)[0] == 't';

    PQclear(Res);

    return Exists;
}

void
create_table_if_not_exists(PGconn *Conn, cJSON *Json)
{
    if(Json == NULL || !cJSON_IsObject(Json))
    {
        printf("Error: Input was not a valid, parsed JSON object.\nAttempting to parse given "
               "object...");
    }

    cJSON *JsonParsed = cJSON_Parse(*Json);

    if(Json == NULL || !cJSON_IsObject(Json))
    {
        printf("Error: Input was not a valid, parsed JSON object.\nAttempting to parse given "
               "object...");
    }
}

int
json_to_create_table(const char *json_string)
{
    int status = 0;

    cJSON *json = cJSON_Parse(json_string); // make sure json_string is zero terminated

    if(json == NULL) // error handling
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if(error_ptr == NULL)
        {
            fprintf(stderr, "ERROR before %s", error_ptr);
        }
        status = 0;
        goto end;
    }

    char *printed = cJSON_Print(json);
    printf("parsed and printed json data: %s", printed);

end:
    cJSON_Delete(json);
    return status;
}
