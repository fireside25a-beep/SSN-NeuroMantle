#define _POSIX_C_SOURCE 200809L

#include <toolchains.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int safe_command_name(const char *name) {
    if (name == NULL || *name == '\0') return 0;
    for (const unsigned char *cursor = (const unsigned char *)name;
         *cursor != '\0'; ++cursor) {
        if (isspace(*cursor)) return 0;
    }
    return 1;
}

int toolchain_find_executable(const char *name, char *out_path, size_t out_size) {
    if (!safe_command_name(name) || out_path == NULL || out_size == 0u) return 0;

    if (strchr(name, '/') != NULL) {
        if (access(name, X_OK) != 0) return 0;
        const int written = snprintf(out_path, out_size, "%s", name);
        return written >= 0 && (size_t)written < out_size;
    }

    const char *path_value = getenv("PATH");
    if (path_value == NULL) return 0;

    const char *cursor = path_value;
    while (1) {
        const char *separator = strchr(cursor, ':');
        const size_t length = separator == NULL ? strlen(cursor) : (size_t)(separator - cursor);
        char directory[TOOLCHAIN_PATH_LENGTH];
        if (length == 0u) {
            (void)snprintf(directory, sizeof(directory), ".");
        } else if (length < sizeof(directory)) {
            memcpy(directory, cursor, length);
            directory[length] = '\0';
        } else {
            directory[0] = '\0';
        }

        if (directory[0] != '\0') {
            char candidate[TOOLCHAIN_PATH_LENGTH];
            const int written = snprintf(candidate, sizeof(candidate), "%s/%s", directory, name);
            if (written >= 0 && (size_t)written < sizeof(candidate) &&
                access(candidate, X_OK) == 0) {
                const int copied = snprintf(out_path, out_size, "%s", candidate);
                return copied >= 0 && (size_t)copied < out_size;
            }
        }

        if (separator == NULL) break;
        cursor = separator + 1;
    }
    return 0;
}

static int resolve_first(
    const char *override_name,
    const char *first,
    const char *second,
    char *out_path,
    size_t out_size
) {
    const char *override_value = override_name == NULL ? NULL : getenv(override_name);
    if (override_value != NULL && *override_value != '\0') {
        return toolchain_find_executable(override_value, out_path, out_size);
    }
    if (first != NULL && toolchain_find_executable(first, out_path, out_size)) return 1;
    return second != NULL && toolchain_find_executable(second, out_path, out_size);
}

int toolchain_resolve(enum project_language language, struct toolchain_info *out_info) {
    if (out_info == NULL) return 0;
    memset(out_info, 0, sizeof(*out_info));
    out_info->language = language;

    switch (language) {
        case PROJECT_LANGUAGE_C:
            out_info->available = resolve_first(
                "NEURO_CC", "clang", "gcc", out_info->primary, sizeof(out_info->primary)
            );
            break;
        case PROJECT_LANGUAGE_CPP:
            out_info->available = resolve_first(
                "NEURO_CXX", "clang++", "g++", out_info->primary, sizeof(out_info->primary)
            );
            break;
        case PROJECT_LANGUAGE_PYTHON:
            out_info->available = resolve_first(
                "NEURO_PYTHON", "python3", NULL,
                out_info->primary, sizeof(out_info->primary)
            );
            break;
        case PROJECT_LANGUAGE_RUST:
            out_info->available = resolve_first(
                "NEURO_CARGO", "cargo", NULL,
                out_info->primary, sizeof(out_info->primary)
            );
            break;
        case PROJECT_LANGUAGE_JAVASCRIPT:
            out_info->available = resolve_first(
                "NEURO_NODE", "node", NULL,
                out_info->primary, sizeof(out_info->primary)
            );
            (void)resolve_first(
                "NEURO_NPM", "npm", NULL,
                out_info->secondary, sizeof(out_info->secondary)
            );
            break;
        case PROJECT_LANGUAGE_GO:
            out_info->available = resolve_first(
                "NEURO_GO", "go", NULL,
                out_info->primary, sizeof(out_info->primary)
            );
            break;
        case PROJECT_LANGUAGE_ZIG:
            out_info->available = resolve_first(
                "NEURO_ZIG", "zig", NULL,
                out_info->primary, sizeof(out_info->primary)
            );
            break;
        case PROJECT_LANGUAGE_JAVA:
            out_info->available = resolve_first(
                "NEURO_JAVAC", "javac", NULL,
                out_info->primary, sizeof(out_info->primary)
            );
            if (!resolve_first(
                    "NEURO_JAVA", "java", NULL,
                    out_info->secondary, sizeof(out_info->secondary))) {
                out_info->available = 0;
            }
            break;
        case PROJECT_LANGUAGE_UNKNOWN:
        case PROJECT_LANGUAGE_COUNT:
            out_info->available = 0;
            break;
    }
    return out_info->available;
}

static void print_candidate(const char *label, const char *command) {
    char path[TOOLCHAIN_PATH_LENGTH];
    if (toolchain_find_executable(command, path, sizeof(path))) {
        printf("  %-10s FOUND   %s\n", label, path);
    } else {
        printf("  %-10s MISSING\n", label);
    }
}

void toolchain_print_status(void) {
    puts("NeuroRoute DevCLI toolchains:");
    print_candidate("clang", "clang");
    print_candidate("gcc", "gcc");
    print_candidate("clang++", "clang++");
    print_candidate("g++", "g++");
    print_candidate("python3", "python3");
    print_candidate("cargo", "cargo");
    print_candidate("node", "node");
    print_candidate("npm", "npm");
    print_candidate("go", "go");
    print_candidate("zig", "zig");
    print_candidate("javac", "javac");
    print_candidate("java", "java");
}
