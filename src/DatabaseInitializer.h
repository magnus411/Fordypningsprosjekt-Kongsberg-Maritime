#ifndef DATABASE_INITIALIZER_H
#define DATABASE_INITIALIZER_H

#include <stdio.h>
#include <stdlib.h>
#include "cjson/cJSON.h"
#include <libpq-fe.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

// Fetches all schema names from the database and returns an array of strings.
cJSON **DbInitGetSchemasFromDb(PGconn *Conn);

// Creates a table with the fields specified in the provided JSON object, if it does not already
// exist.

cJSON *DbInitGetSchemaFromFile(const char *filename);

char *DbInitCreateTableCreationQuery(PGconn *Conn, cJSON *Json);

#endif // DATABASE_INITIALIZER_H
