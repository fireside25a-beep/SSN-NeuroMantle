#ifndef NEUROROUTE_PROJECT_INIT_H
#define NEUROROUTE_PROJECT_INIT_H

#include <stdio.h>

#include <project_detect.h>

int project_init_create(
    enum project_language language,
    const char *directory,
    FILE *output
);

#endif
