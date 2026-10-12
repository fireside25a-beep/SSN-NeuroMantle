#ifndef NEUROROUTE_PROJECT_ACTIONS_H
#define NEUROROUTE_PROJECT_ACTIONS_H

#include <project_detect.h>

int project_action_build(const char *path, enum project_language language);
int project_action_run(const char *path, enum project_language language);
int project_action_test(const char *path, enum project_language language);
int project_action_clean(const char *path, enum project_language language);

#endif
