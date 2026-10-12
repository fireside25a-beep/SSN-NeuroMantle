#ifndef NEUROROUTE_PROJECT_DETECT_H
#define NEUROROUTE_PROJECT_DETECT_H

#include <stddef.h>

enum project_language {
    PROJECT_LANGUAGE_UNKNOWN = 0,
    PROJECT_LANGUAGE_C,
    PROJECT_LANGUAGE_CPP,
    PROJECT_LANGUAGE_PYTHON,
    PROJECT_LANGUAGE_RUST,
    PROJECT_LANGUAGE_JAVASCRIPT,
    PROJECT_LANGUAGE_GO,
    PROJECT_LANGUAGE_ZIG,
    PROJECT_LANGUAGE_JAVA,
    PROJECT_LANGUAGE_COUNT
};

#define PROJECT_DETECT_MAX_EVIDENCE 16u
#define PROJECT_DETECT_EVIDENCE_LENGTH 96u

struct project_detection {
    enum project_language language;
    unsigned int language_mask;
    size_t language_count;
    unsigned char priorities[PROJECT_LANGUAGE_COUNT];
    char evidence[PROJECT_DETECT_MAX_EVIDENCE][PROJECT_DETECT_EVIDENCE_LENGTH];
    size_t evidence_count;
};

const char *project_language_name(enum project_language language);
enum project_language project_language_parse(const char *text);
int project_detect_path(const char *path, struct project_detection *out_result);
void project_detect_print(const struct project_detection *result);

#endif
