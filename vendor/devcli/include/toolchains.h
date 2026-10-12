#ifndef NEUROROUTE_TOOLCHAINS_H
#define NEUROROUTE_TOOLCHAINS_H

#include <stddef.h>

#include <project_detect.h>

#define TOOLCHAIN_PATH_LENGTH 512u

struct toolchain_info {
    enum project_language language;
    int available;
    char primary[TOOLCHAIN_PATH_LENGTH];
    char secondary[TOOLCHAIN_PATH_LENGTH];
};

int toolchain_find_executable(const char *name, char *out_path, size_t out_size);
int toolchain_resolve(enum project_language language, struct toolchain_info *out_info);
void toolchain_print_status(void);

#endif
