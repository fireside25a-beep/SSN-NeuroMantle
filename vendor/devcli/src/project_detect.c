#define _POSIX_C_SOURCE 200809L

#include <project_detect.h>

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include <fs_utils.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

const char *project_language_name(enum project_language language) {
    switch (language) {
        case PROJECT_LANGUAGE_C: return "C";
        case PROJECT_LANGUAGE_CPP: return "C++";
        case PROJECT_LANGUAGE_PYTHON: return "Python";
        case PROJECT_LANGUAGE_RUST: return "Rust";
        case PROJECT_LANGUAGE_JAVASCRIPT: return "JavaScript";
        case PROJECT_LANGUAGE_GO: return "Go";
        case PROJECT_LANGUAGE_ZIG: return "Zig";
        case PROJECT_LANGUAGE_JAVA: return "Java";
        case PROJECT_LANGUAGE_UNKNOWN: return "Unknown";
        case PROJECT_LANGUAGE_COUNT: return "Invalid";
    }
    return "Invalid";
}

enum project_language project_language_parse(const char *text) {
    if (text == NULL) {
        return PROJECT_LANGUAGE_UNKNOWN;
    }
    if (strcasecmp(text, "c") == 0) return PROJECT_LANGUAGE_C;
    if (strcasecmp(text, "c++") == 0 || strcasecmp(text, "cpp") == 0 ||
        strcasecmp(text, "cxx") == 0) return PROJECT_LANGUAGE_CPP;
    if (strcasecmp(text, "python") == 0 || strcasecmp(text, "py") == 0)
        return PROJECT_LANGUAGE_PYTHON;
    if (strcasecmp(text, "rust") == 0 || strcasecmp(text, "rs") == 0)
        return PROJECT_LANGUAGE_RUST;
    if (strcasecmp(text, "javascript") == 0 || strcasecmp(text, "js") == 0 ||
        strcasecmp(text, "node") == 0) return PROJECT_LANGUAGE_JAVASCRIPT;
    if (strcasecmp(text, "go") == 0 || strcasecmp(text, "golang") == 0)
        return PROJECT_LANGUAGE_GO;
    if (strcasecmp(text, "zig") == 0) return PROJECT_LANGUAGE_ZIG;
    if (strcasecmp(text, "java") == 0) return PROJECT_LANGUAGE_JAVA;
    return PROJECT_LANGUAGE_UNKNOWN;
}

static void add_evidence(
    struct project_detection *result,
    enum project_language language,
    const char *name,
    unsigned char priority
) {
    if (priority > result->priorities[language]) {
        result->priorities[language] = priority;
    }
    if (result->evidence_count < PROJECT_DETECT_MAX_EVIDENCE) {
        (void)snprintf(
            result->evidence[result->evidence_count],
            PROJECT_DETECT_EVIDENCE_LENGTH,
            "%s: %s",
            project_language_name(language),
            name
        );
        result->evidence_count += 1;
    }
}

static int has_suffix(const char *name, const char *suffix) {
    const size_t name_length = strlen(name);
    const size_t suffix_length = strlen(suffix);
    return name_length >= suffix_length &&
        strcasecmp(name + name_length - suffix_length, suffix) == 0;
}

static void inspect_name(
    struct project_detection *result,
    const char *name,
    int root_level
) {
    if (root_level) {
        if (strcmp(name, "Makefile") == 0) add_evidence(result, PROJECT_LANGUAGE_C, name, 1u);
        if (strcmp(name, "CMakeLists.txt") == 0)
            add_evidence(result, PROJECT_LANGUAGE_CPP, name, 3u);
        if (strcmp(name, "pyproject.toml") == 0 ||
            strcmp(name, "requirements.txt") == 0)
            add_evidence(result, PROJECT_LANGUAGE_PYTHON, name, 3u);
        if (strcmp(name, "Cargo.toml") == 0)
            add_evidence(result, PROJECT_LANGUAGE_RUST, name, 3u);
        if (strcmp(name, "package.json") == 0)
            add_evidence(result, PROJECT_LANGUAGE_JAVASCRIPT, name, 3u);
        if (strcmp(name, "go.mod") == 0)
            add_evidence(result, PROJECT_LANGUAGE_GO, name, 3u);
        if (strcmp(name, "build.zig") == 0)
            add_evidence(result, PROJECT_LANGUAGE_ZIG, name, 3u);
        if (strcmp(name, "pom.xml") == 0 || strcmp(name, "build.gradle") == 0 ||
            strcmp(name, "build.gradle.kts") == 0)
            add_evidence(result, PROJECT_LANGUAGE_JAVA, name, 3u);
    }

    if (has_suffix(name, ".c")) add_evidence(result, PROJECT_LANGUAGE_C, name, 2u);
    if (has_suffix(name, ".cpp") || has_suffix(name, ".cc") || has_suffix(name, ".cxx"))
        add_evidence(result, PROJECT_LANGUAGE_CPP, name, 2u);
    if (has_suffix(name, ".py")) add_evidence(result, PROJECT_LANGUAGE_PYTHON, name, 2u);
    if (has_suffix(name, ".rs")) add_evidence(result, PROJECT_LANGUAGE_RUST, name, 2u);
    if (has_suffix(name, ".js") || has_suffix(name, ".mjs") || has_suffix(name, ".cjs"))
        add_evidence(result, PROJECT_LANGUAGE_JAVASCRIPT, name, 2u);
    if (has_suffix(name, ".go")) add_evidence(result, PROJECT_LANGUAGE_GO, name, 2u);
    if (has_suffix(name, ".zig")) add_evidence(result, PROJECT_LANGUAGE_ZIG, name, 2u);
    if (has_suffix(name, ".java")) add_evidence(result, PROJECT_LANGUAGE_JAVA, name, 2u);
}

static int ignored_directory(const char *name) {
    static const char *const ignored[] = {
        ".git", ".neuro", "build", "bin", "lib", "node_modules", "target",
        "zig-cache", ".zig-cache", "zig-out", "__pycache__"
    };
    for (size_t i = 0; i < sizeof(ignored) / sizeof(ignored[0]); ++i) {
        if (strcmp(name, ignored[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int scan_directory(
    const char *path,
    unsigned int depth,
    struct project_detection *result
) {
    DIR *directory = opendir(path);
    if (directory == NULL) {
        return -1;
    }

    int scan_result = 0;
    struct dirent *entry = NULL;
    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char full_path[PATH_MAX];
        if (fs_join(full_path, sizeof(full_path), path, entry->d_name) != 0) {
            scan_result = -1;
            break;
        }

        struct stat status;
        if (lstat(full_path, &status) != 0) {
            scan_result = -1;
            break;
        }

        if (S_ISREG(status.st_mode)) {
            inspect_name(result, entry->d_name, depth == 0u);
        } else if (S_ISDIR(status.st_mode) && !S_ISLNK(status.st_mode) &&
                   depth < 2u && !ignored_directory(entry->d_name)) {
            if (scan_directory(full_path, depth + 1u, result) != 0) {
                scan_result = -1;
                break;
            }
        }
    }

    closedir(directory);
    return scan_result;
}

int project_detect_path(const char *path, struct project_detection *out_result) {
    if (path == NULL || out_result == NULL) {
        return -1;
    }
    memset(out_result, 0, sizeof(*out_result));

    struct stat status;
    if (stat(path, &status) != 0) {
        return -1;
    }
    if (S_ISREG(status.st_mode)) {
        const char *name = strrchr(path, '/');
        inspect_name(out_result, name == NULL ? path : name + 1, 1);
    } else if (S_ISDIR(status.st_mode)) {
        if (scan_directory(path, 0u, out_result) != 0) {
            return -1;
        }
    } else {
        errno = ENOTDIR;
        return -1;
    }

    unsigned char highest_priority = 0u;
    for (enum project_language language = PROJECT_LANGUAGE_C;
         language < PROJECT_LANGUAGE_COUNT;
         language = (enum project_language)(language + 1)) {
        if (out_result->priorities[language] > highest_priority) {
            highest_priority = out_result->priorities[language];
        }
    }

    for (enum project_language language = PROJECT_LANGUAGE_C;
         language < PROJECT_LANGUAGE_COUNT;
         language = (enum project_language)(language + 1)) {
        if (highest_priority != 0u &&
            out_result->priorities[language] == highest_priority) {
            out_result->language_mask |= 1u << (unsigned int)language;
            out_result->language_count += 1u;
            out_result->language = language;
        }
    }
    if (out_result->language_count != 1u) {
        out_result->language = PROJECT_LANGUAGE_UNKNOWN;
    }
    return 0;
}

void project_detect_print(const struct project_detection *result) {
    if (result == NULL) {
        return;
    }
    if (result->language_count == 0u) {
        puts("Project type: unknown");
    } else if (result->language_count == 1u) {
        printf("Project type: %s\n", project_language_name(result->language));
    } else {
        printf("Project type: ambiguous (%zu languages)\n", result->language_count);
    }
    for (size_t i = 0; i < result->evidence_count; ++i) {
        printf("  evidence: %s\n", result->evidence[i]);
    }
}
