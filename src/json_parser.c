#include <stdio.h>
#include <stdlib.h>
#include "cjson/cJSON.h"
#include <libpq-fe.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

char *
get_name_from_json(cJSON *Json)
{
    cJSON *Name = cJSON_GetObjectItemCaseSensitive(Json, "name");
    if(cJSON_IsString(Name) && (Name->valuestring != NULL))
    {
        return strdup(Name->valuestring);
    }
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

    int    RowCount     = PQntuples(Res);
    char **NameArray    = malloc((RowCount + 1) * sizeof(char *));
    NameArray[RowCount] = NULL;

    for(int i = 0; i < RowCount; i++)
    {
        const char *SchemaJsonString = PQgetvalue(Res, i, 1);
        cJSON      *SchemaJson       = cJSON_Parse(SchemaJsonString);

        if(SchemaJson == NULL)
        {
            fprintf(stderr, "Error parsing JSON: %s\n", cJSON_GetErrorPtr());
            continue;
        }

        NameArray[i] = get_name_from_json(SchemaJson);
        cJSON_Delete(SchemaJson);
    }

    PQclear(Res);
    return NameArray;
}

bool
check_if_table_exists(PGconn *Conn, const char *Name)
{
    char  *QueryTemplate = "SELECT EXISTS (SELECT 1 FROM pg_tables WHERE schemaname = 'public' AND "
                           "tablename = '%s')";
    size_t QueryLength   = snprintf(NULL, 0, QueryTemplate, Name);
    char  *Query         = malloc(QueryLength + 1);

    if(Query == NULL)
    {
        fprintf(stderr, "Memory allocation failed");
        return false;
    }

    snprintf(Query, QueryLength + 1, QueryTemplate, Name);
    PGresult *Res = PQexec(Conn, Query);
    free(Query);

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

char *
create_table_if_not_exists(PGconn *Conn, cJSON *Json)
{
    if(Json == NULL || !cJSON_IsObject(Json))
    {
        printf("Error: Input was not a valid, parsed JSON object.\n");
        exit(1);
    }

    cJSON *TableNameItem = cJSON_GetObjectItem(Json, "name");
    if(TableNameItem == NULL || !cJSON_IsString(TableNameItem))
    {
        printf("ERROR: JSON does not contain a \"name\" field\n");
        exit(1);
    }
    const char *Name = TableNameItem->valuestring;

    char *Query = malloc(2048);
    if(Query == NULL)
    {
        printf("Memory allocation failed");
        exit(1);
    }

    sprintf(Query, "CREATE TABLE %s (\n id SERIAL PRIMARY KEY, \n", Name);

    cJSON *CurrentElement = NULL;
    int    FieldCount     = 0;

    cJSON_ArrayForEach(CurrentElement, Json)
    {
        if(strcmp(CurrentElement->string, "name") == 0)
        {
            continue;
        }
        if(cJSON_IsString(CurrentElement))
        {
            char *Type = CurrentElement->valuestring;
            for(char *p = Type; *p; ++p)
            {
                *p = toupper(*p);
            }

            FieldCount++;
            char Field[256];
            sprintf(Field, "%s %s", CurrentElement->string, Type);
            strcat(Query, Field);
            strcat(Query, ", \n");
        }
    }

    if(FieldCount > 0)
    {
        Query[strlen(Query) - 2] = '\0';
    }
    strcat(Query, "\n);");
    return Query;
}

void
all_together_now(PGconn *Conn)
{
    char **SchemaNames = get_all_schema_names_from_db(Conn);

    for(int i = 0; SchemaNames[i] != NULL; ++i)
    {
        if(check_if_table_exists(Conn, SchemaNames[i]))
        {
            printf("Table '%s' already exists, skipping...\n", SchemaNames[i]);
            continue;
        }
        else
        {
            printf("Table '%s' does not exist. Creating...\n", SchemaNames[i]);

            cJSON    *SchemaJson  = cJSON_Parse(SchemaNames[i]);
            char     *CreateQuery = create_table_if_not_exists(Conn, SchemaJson);
            PGresult *CreateRes   = PQexec(Conn, CreateQuery);

            if(PQresultStatus(CreateRes) != PGRES_COMMAND_OK)
            {
                fprintf(stderr, "Table creation failed: %s\n", PQerrorMessage(Conn));
            }
            else
            {
                printf("Table '%s' created successfully.\n", SchemaNames[i]);
            }
            PQclear(CreateRes);
            free(CreateQuery);
            cJSON_Delete(SchemaJson);
        }
        free(SchemaNames[i]);
    }
    free(SchemaNames);
}
