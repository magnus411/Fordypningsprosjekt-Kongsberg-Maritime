#include <stdio.h>
#include <stdlib.h>
#include <libpq-fe.h>
#include "DatabaseInitializer.h"
#include "cjson/cJSON.h"

int
main(void)
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
        char  *CreateQuery = CreateTableCreationQuery(Conn, Schema);
        printf("Query: %s\n", CreateQuery);
        free(CreateQuery);
        }

    PQfinish(Conn);
    return 0;
}
