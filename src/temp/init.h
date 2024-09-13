#ifndef INIT_H
#define INIT_H

#include <libpq-fe.h>

typedef struct {
    int id;
    char *name;
    int sample_rate;
    char *variables;
} Template;

void initialize(PGconn *conn, Template **templates, int *template_count);

void free_templates(Template *templates, int template_count);

#endif 
