#ifndef SENSOR_SCHEMA_H
#define SENSOR_SCHEMA_H

#include <stdio.h>
#include <stdlib.h>
#include "cjson/cJSON.h"
#include <libpq-fe.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>

// Function declarations

// Parses JSON and returns the "name" field as a string.
char *get_name_from_json(cJSON *Json);

// Fetches all schema names from the database and returns an array of strings.
char **get_all_schema_names_from_db(PGconn *Conn);

// Checks if a table with the given name exists in the database.
bool check_if_table_exists(PGconn *Conn, const char *Name);

// Creates a table with the fields specified in the provided JSON object, if it does not already
// exist.
char *create_table_if_not_exists(PGconn *Conn, cJSON *Json);

// Main function to retrieve schema names, check table existence, and create tables if necessary.
void all_together_now(PGconn *Conn);

#endif // SENSOR_SCHEMA_H
