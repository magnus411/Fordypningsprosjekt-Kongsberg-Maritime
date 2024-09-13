#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <libpq-fe.h>



typedef struct {
    int id;
    char *name;
    int sample_rate;
    char *variables;

} Template;



void initialize(PGconn *conn, Template **templates, int *template_count) {
  
    const char *query = "SELECT id, name, sample_rate, variables FROM sensortemplates";
    PGresult *res = PQexec(conn, query);


    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        fprintf(stderr, "Failed to execute query: %s", PQerrorMessage(conn));
        PQclear(res);
        return;

    }



    *template_count = PQntuples(res);
    *templates = (Template *)malloc(*template_count * sizeof(Template));

    for (int i = 0; i < *template_count; i++) {

        (*templates)[i].id = atoi(PQgetvalue(res, i, 0));  
        (*templates)[i].name = strdup(PQgetvalue(res, i, 1));  
        (*templates)[i].sample_rate = atoi(PQgetvalue(res, i, 2));  
        (*templates)[i].variables = strdup(PQgetvalue(res, i, 3));  
    }

    PQclear(res);

}



void free_templates(Template *templates, int template_count) {

    for (int i = 0; i < template_count; i++) {
        free(templates[i].name);  
        free(templates[i].variables);  
    }

    free(templates);  
}

