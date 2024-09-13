#include <stdio.h>
#include "db_connection.h"

#include "init.h"
int main() {
    //Change this ofc
    const char *conninfo = "host=localhost dbname=postgres user=postgres password=password";

    PGconn *conn = connect_db(conninfo);
    printf("Connected!");
   
    Template *templates = NULL;
    int template_count = 0;
    initialize(conn, &templates, &template_count);

    for (int i = 0; i < template_count; i++) {
        printf("Template ID: %d, Name: %s, Sample Rate: %d, Variables: %s\n", 
            templates[i].id, templates[i].name, templates[i].sample_rate, templates[i].variables);
    }

    free_templates(templates, template_count);


    disconnect_db(conn);

    return 0;
}


