#define _POSIX_C_SOURCE 200809L

#include <project_actions.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <fs_utils.h>
#include <process_runner.h>
#include <toolchains.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static int joined_exists(const char *path, const char *name) {
    char full_path[PATH_MAX];
    return fs_join(full_path, sizeof(full_path), path, name) == 0 &&
        fs_path_exists(full_path);
}

static int require_file(const char *path, const char *name) {
    char full_path[PATH_MAX];
    if (fs_join(full_path, sizeof(full_path), path, name) != 0 ||
        !fs_file_exists(full_path)) {
        fprintf(stderr, "neuro: required project file not found: %s/%s\n", path, name);
        return 0;
    }
    return 1;
}

static int ensure_neuro_directory(const char *path, const char *suffix) {
    char directory[PATH_MAX];
    if (fs_join(directory, sizeof(directory), path, suffix) != 0 ||
        fs_mkdirs(directory) != 0) {
        fprintf(stderr, "neuro: cannot create build directory: %s/%s\n", path, suffix);
        return 0;
    }
    return 1;
}

static int resolve_or_report(
    enum project_language language,
    struct toolchain_info *toolchain
) {
    if (toolchain_resolve(language, toolchain)) {
        return 1;
    }
    fprintf(
        stderr,
        "neuro: required %s toolchain is not installed or not executable\n",
        project_language_name(language)
    );
    return 0;
}

static int run_make(const char *path, const char *target) {
    char make_path[TOOLCHAIN_PATH_LENGTH];
    if (!toolchain_find_executable("make", make_path, sizeof(make_path))) {
        fprintf(stderr, "neuro: required build tool is missing: make\n");
        return 127;
    }
    char *argv_default[] = {make_path, NULL};
    char *argv_target[] = {make_path, (char *)target, NULL};
    return process_run(path, target == NULL ? argv_default : argv_target);
}

static int build_c_family(
    const char *path,
    enum project_language language,
    const char *source_name,
    const char *output_name
) {
    if (joined_exists(path, "Makefile")) {
        return run_make(path, NULL);
    }
    if (!require_file(path, source_name) || !ensure_neuro_directory(path, ".neuro")) {
        return 1;
    }

    struct toolchain_info toolchain;
    if (!resolve_or_report(language, &toolchain)) {
        return 127;
    }

    char output_argument[PATH_MAX];
    const int written = snprintf(output_argument, sizeof(output_argument), ".neuro/%s", output_name);
    if (written < 0 || (size_t)written >= sizeof(output_argument)) {
        return 1;
    }

    if (language == PROJECT_LANGUAGE_C) {
        char *argv[] = {
            toolchain.primary,
            "-std=c11",
            "-Wall",
            "-Wextra",
            "-Wpedantic",
            "-Werror",
            (char *)source_name,
            "-o",
            output_argument,
            NULL
        };
        return process_run(path, argv);
    }

    char *argv[] = {
        toolchain.primary,
        "-std=c++17",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Werror",
        (char *)source_name,
        "-o",
        output_argument,
        NULL
    };
    return process_run(path, argv);
}

static int build_python(const char *path) {
    if (!require_file(path, "main.py")) return 1;
    struct toolchain_info toolchain;
    if (!resolve_or_report(PROJECT_LANGUAGE_PYTHON, &toolchain)) return 127;
    char *argv[] = {toolchain.primary, "-m", "py_compile", "main.py", NULL};
    return process_run(path, argv);
}

static int build_rust(const char *path) {
    if (!require_file(path, "Cargo.toml")) return 1;
    struct toolchain_info toolchain;
    if (!resolve_or_report(PROJECT_LANGUAGE_RUST, &toolchain)) return 127;
    char *argv[] = {toolchain.primary, "build", NULL};
    return process_run(path, argv);
}

static int build_javascript(const char *path) {
    struct toolchain_info toolchain;
    if (!resolve_or_report(PROJECT_LANGUAGE_JAVASCRIPT, &toolchain)) return 127;

    if (joined_exists(path, "package.json") && toolchain.secondary[0] != '\0') {
        char *argv[] = {toolchain.secondary, "run", "build", "--if-present", NULL};
        const int status = process_run(path, argv);
        if (status != 0) return status;
    }
    if (!require_file(path, "main.js")) return 1;
    char *argv[] = {toolchain.primary, "--check", "main.js", NULL};
    return process_run(path, argv);
}

static int build_go(const char *path) {
    if (!require_file(path, "go.mod")) return 1;
    if (!ensure_neuro_directory(path, ".neuro")) return 1;
    struct toolchain_info toolchain;
    if (!resolve_or_report(PROJECT_LANGUAGE_GO, &toolchain)) return 127;
    char *argv[] = {toolchain.primary, "build", "-o", ".neuro/neuro-go-app", ".", NULL};
    return process_run(path, argv);
}

static int build_zig(const char *path) {
    struct toolchain_info toolchain;
    if (!resolve_or_report(PROJECT_LANGUAGE_ZIG, &toolchain)) return 127;
    if (joined_exists(path, "build.zig")) {
        char *argv[] = {toolchain.primary, "build", NULL};
        return process_run(path, argv);
    }
    if (!require_file(path, "main.zig") || !ensure_neuro_directory(path, ".neuro")) {
        return 1;
    }
    char *argv[] = {
        toolchain.primary,
        "build-exe",
        "main.zig",
        "-femit-bin=.neuro/neuro-zig-app",
        NULL
    };
    return process_run(path, argv);
}

static int build_java(const char *path) {
    if (!require_file(path, "Main.java") || !ensure_neuro_directory(path, ".neuro/java")) {
        return 1;
    }
    struct toolchain_info toolchain;
    if (!resolve_or_report(PROJECT_LANGUAGE_JAVA, &toolchain)) return 127;
    char *argv[] = {toolchain.primary, "-d", ".neuro/java", "Main.java", NULL};
    return process_run(path, argv);
}

int project_action_build(const char *path, enum project_language language) {
    switch (language) {
        case PROJECT_LANGUAGE_C:
            return build_c_family(path, language, "main.c", "neuro-c-app");
        case PROJECT_LANGUAGE_CPP:
            return build_c_family(path, language, "main.cpp", "neuro-cpp-app");
        case PROJECT_LANGUAGE_PYTHON: return build_python(path);
        case PROJECT_LANGUAGE_RUST: return build_rust(path);
        case PROJECT_LANGUAGE_JAVASCRIPT: return build_javascript(path);
        case PROJECT_LANGUAGE_GO: return build_go(path);
        case PROJECT_LANGUAGE_ZIG: return build_zig(path);
        case PROJECT_LANGUAGE_JAVA: return build_java(path);
        case PROJECT_LANGUAGE_UNKNOWN:
        case PROJECT_LANGUAGE_COUNT:
            break;
    }
    fprintf(stderr, "neuro: cannot build an unknown project type\n");
    return 2;
}

static int run_built_binary(
    const char *path,
    enum project_language language,
    const char *relative_binary
) {
    if (!joined_exists(path, relative_binary)) {
        const int build_status = project_action_build(path, language);
        if (build_status != 0) return build_status;
    }
    char executable[PATH_MAX];
    const int written = snprintf(executable, sizeof(executable), "./%s", relative_binary);
    if (written < 0 || (size_t)written >= sizeof(executable)) return 1;
    char *argv[] = {executable, NULL};
    return process_run(path, argv);
}

int project_action_run(const char *path, enum project_language language) {
    struct toolchain_info toolchain;
    switch (language) {
        case PROJECT_LANGUAGE_C:
            if (joined_exists(path, "Makefile")) {
                fprintf(stderr, "neuro: run is undefined for this Makefile project; use 'make demo' or the project-specific target\n");
                return 2;
            }
            return run_built_binary(path, language, ".neuro/neuro-c-app");
        case PROJECT_LANGUAGE_CPP:
            if (joined_exists(path, "Makefile")) {
                fprintf(stderr, "neuro: run is undefined for this Makefile project; use its documented run target\n");
                return 2;
            }
            return run_built_binary(path, language, ".neuro/neuro-cpp-app");
        case PROJECT_LANGUAGE_PYTHON: {
            if (!require_file(path, "main.py")) return 1;
            if (!resolve_or_report(language, &toolchain)) return 127;
            char *argv[] = {toolchain.primary, "main.py", NULL};
            return process_run(path, argv);
        }
        case PROJECT_LANGUAGE_RUST: {
            if (!require_file(path, "Cargo.toml")) return 1;
            if (!resolve_or_report(language, &toolchain)) return 127;
            char *argv[] = {toolchain.primary, "run", "--quiet", NULL};
            return process_run(path, argv);
        }
        case PROJECT_LANGUAGE_JAVASCRIPT: {
            if (!require_file(path, "main.js")) return 1;
            if (!resolve_or_report(language, &toolchain)) return 127;
            char *argv[] = {toolchain.primary, "main.js", NULL};
            return process_run(path, argv);
        }
        case PROJECT_LANGUAGE_GO:
            return run_built_binary(path, language, ".neuro/neuro-go-app");
        case PROJECT_LANGUAGE_ZIG:
            if (joined_exists(path, "build.zig")) {
                if (!resolve_or_report(language, &toolchain)) return 127;
                char *argv[] = {toolchain.primary, "build", "run", NULL};
                return process_run(path, argv);
            }
            return run_built_binary(path, language, ".neuro/neuro-zig-app");
        case PROJECT_LANGUAGE_JAVA: {
            const int status = project_action_build(path, language);
            if (status != 0) return status;
            if (!resolve_or_report(language, &toolchain)) return 127;
            char *argv[] = {toolchain.secondary, "-cp", ".neuro/java", "Main", NULL};
            return process_run(path, argv);
        }
        case PROJECT_LANGUAGE_UNKNOWN:
        case PROJECT_LANGUAGE_COUNT:
            break;
    }
    fprintf(stderr, "neuro: cannot run an unknown project type\n");
    return 2;
}

int project_action_test(const char *path, enum project_language language) {
    struct toolchain_info toolchain;
    switch (language) {
        case PROJECT_LANGUAGE_C:
        case PROJECT_LANGUAGE_CPP:
            if (joined_exists(path, "Makefile")) return run_make(path, "test");
            {
                const int build_status = project_action_build(path, language);
                return build_status == 0 ? project_action_run(path, language) : build_status;
            }
        case PROJECT_LANGUAGE_PYTHON:
            if (!resolve_or_report(language, &toolchain)) return 127;
            if (joined_exists(path, "tests")) {
                char *argv[] = {
                    toolchain.primary, "-m", "unittest", "discover", "-s", "tests", NULL
                };
                return process_run(path, argv);
            }
            {
                const int build_status = project_action_build(path, language);
                return build_status == 0 ? project_action_run(path, language) : build_status;
            }
        case PROJECT_LANGUAGE_RUST:
            if (!resolve_or_report(language, &toolchain)) return 127;
            {
                char *argv[] = {toolchain.primary, "test", NULL};
                return process_run(path, argv);
            }
        case PROJECT_LANGUAGE_JAVASCRIPT:
            if (!resolve_or_report(language, &toolchain)) return 127;
            if (joined_exists(path, "package.json") && toolchain.secondary[0] != '\0') {
                char *argv[] = {toolchain.secondary, "test", "--if-present", NULL};
                return process_run(path, argv);
            }
            {
                const int build_status = project_action_build(path, language);
                return build_status == 0 ? project_action_run(path, language) : build_status;
            }
        case PROJECT_LANGUAGE_GO:
            if (!resolve_or_report(language, &toolchain)) return 127;
            {
                char *argv[] = {toolchain.primary, "test", "./...", NULL};
                return process_run(path, argv);
            }
        case PROJECT_LANGUAGE_ZIG:
            if (!resolve_or_report(language, &toolchain)) return 127;
            if (joined_exists(path, "build.zig")) {
                char *argv[] = {toolchain.primary, "build", "test", NULL};
                return process_run(path, argv);
            }
            if (!require_file(path, "main.zig")) return 1;
            {
                char *argv[] = {toolchain.primary, "test", "main.zig", NULL};
                return process_run(path, argv);
            }
        case PROJECT_LANGUAGE_JAVA: {
            const int build_status = project_action_build(path, language);
            return build_status == 0 ? project_action_run(path, language) : build_status;
        }
        case PROJECT_LANGUAGE_UNKNOWN:
        case PROJECT_LANGUAGE_COUNT:
            break;
    }
    fprintf(stderr, "neuro: cannot test an unknown project type\n");
    return 2;
}

static int remove_if_present(const char *path, const char *name) {
    char full_path[PATH_MAX];
    if (fs_join(full_path, sizeof(full_path), path, name) != 0) return -1;
    return fs_remove_tree(full_path);
}

int project_action_clean(const char *path, enum project_language language) {
    int status = 0;
    struct toolchain_info toolchain;
    switch (language) {
        case PROJECT_LANGUAGE_C:
        case PROJECT_LANGUAGE_CPP:
            if (joined_exists(path, "Makefile")) status = run_make(path, "clean");
            break;
        case PROJECT_LANGUAGE_PYTHON:
            if (fs_remove_python_cache(path) != 0) status = 1;
            break;
        case PROJECT_LANGUAGE_RUST:
            if (resolve_or_report(language, &toolchain)) {
                char *argv[] = {toolchain.primary, "clean", NULL};
                status = process_run(path, argv);
            } else {
                status = 127;
            }
            break;
        case PROJECT_LANGUAGE_JAVASCRIPT:
            if (joined_exists(path, "package.json") &&
                resolve_or_report(language, &toolchain) && toolchain.secondary[0] != '\0') {
                char *argv[] = {toolchain.secondary, "run", "clean", "--if-present", NULL};
                status = process_run(path, argv);
            }
            break;
        case PROJECT_LANGUAGE_GO:
            if (resolve_or_report(language, &toolchain)) {
                char *argv[] = {toolchain.primary, "clean", NULL};
                status = process_run(path, argv);
            } else {
                status = 127;
            }
            break;
        case PROJECT_LANGUAGE_ZIG:
            if (remove_if_present(path, ".zig-cache") != 0 ||
                remove_if_present(path, "zig-cache") != 0 ||
                remove_if_present(path, "zig-out") != 0) status = 1;
            break;
        case PROJECT_LANGUAGE_JAVA:
            break;
        case PROJECT_LANGUAGE_UNKNOWN:
        case PROJECT_LANGUAGE_COUNT:
            fprintf(stderr, "neuro: cannot clean an unknown project type\n");
            return 2;
    }
    if (remove_if_present(path, ".neuro") != 0 && status == 0) status = 1;
    return status;
}
