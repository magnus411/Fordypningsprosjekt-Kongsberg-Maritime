#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>

#include "cjson/cJSON.h"
#include <DatabaseInitializer.h>

#define SDB_LOG_LEVEL 4
#include <SdbExtern.h>

SDB_LOG_REGISTER(InitDbTest);

int
TestDbInit(void)
{
    // Connect to the database
    PGconn *Conn
        = PQconnectdb("host=localhost port=5432 dbname=postgres user=postgres password=password");

    if(PQstatus(Conn) != CONNECTION_OK) {
        fprintf(stderr, "Connection to database failed: %s", PQerrorMessage(Conn));
        PQfinish(Conn);
        return 1;
    }

    cJSON **SchemaObjects = DbInitGetSchemasFromDb(Conn);

    for(int i = 0; SchemaObjects[i] != NULL; ++i) {
        cJSON *Schema      = SchemaObjects[i];
        char  *CreateQuery = DbInitCreateTableCreationQuery(Conn, Schema);
        printf("Query: %s\n", CreateQuery);

        PGresult *CreateRes = PQexec(Conn, CreateQuery);
        if(PQresultStatus(CreateRes) != PGRES_COMMAND_OK) {
            fprintf(stderr, "Table creation failed: %s\n", PQerrorMessage(Conn));
        } else {
            printf("Table '%s' created successfully.\n", SchemaObjects[i]->valuestring);
        }
        PQclear(CreateRes);
        free(CreateQuery);
        cJSON_Delete(Schema);

        free(SchemaObjects[i]);
    }
    free(SchemaObjects);

    const char *filename = "shaft_power.json";

    cJSON *SchemaFromFile = DbInitGetSchemaFromFile(filename);
    char  *CreateQuery2   = DbInitCreateTableCreationQuery(Conn, SchemaFromFile);
    printf("Query: %s\n", CreateQuery2);

    PQfinish(Conn);
    return 0;
}