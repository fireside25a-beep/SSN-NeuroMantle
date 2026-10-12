#define _POSIX_C_SOURCE 200809L

#include <devcli.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fs_utils.h>
#include <process_runner.h>
#include <project_actions.h>
#include <project_detect.h>
#include <project_init.h>
#include <toolchains.h>

#define INTERACTIVE_MAX_ARGUMENTS 32

static void print_help(FILE *output) {
    fputs(
        "NeuroRoute DevCLI v0.4.0\n"
        "Usage:\n"
        "  neuro help\n"
        "  neuro status\n"
        "  neuro detect [path]\n"
        "  neuro init <language> <directory>\n"
        "  neuro build [path]\n"
        "  neuro run [path]\n"
        "  neuro test [path]\n"
        "  neuro clean [path]\n"
        "  neuro edit <file>\n"
        "\n"
        "Languages: c, cpp, python, rust, javascript, go, zig, java\n"
        "Run without arguments for interactive mode.\n",
        output
    );
}

static int detect_language_for_action(
    const char *path,
    enum project_language *out_language
) {
    struct project_detection detection;
    if (project_detect_path(path, &detection) != 0) {
        fprintf(stderr, "neuro: cannot inspect '%s': %s\n", path, strerror(errno));
        return 1;
    }
    if (detection.language_count == 0u) {
        fprintf(stderr, "neuro: no supported project type detected in '%s'\n", path);
        return 3;
    }
    if (detection.language_count > 1u) {
        fprintf(stderr, "neuro: ambiguous project type in '%s'\n", path);
        project_detect_print(&detection);
        return 3;
    }
    *out_language = detection.language;
    return 0;
}

static int command_detect(const char *path) {
    struct project_detection detection;
    if (project_detect_path(path, &detection) != 0) {
        fprintf(stderr, "neuro: cannot inspect '%s': %s\n", path, strerror(errno));
        return 1;
    }
    printf("Path: %s\n", path);
    project_detect_print(&detection);
    return detection.language_count > 1u ? 3 : 0;
}

static int command_edit(const char *file) {
    if (!fs_file_exists(file)) {
        fprintf(stderr, "neuro: file not found: %s\n", file);
        return 1;
    }

    const char *editor = getenv("VISUAL");
    if (editor == NULL || *editor == '\0') editor = getenv("EDITOR");
    if (editor == NULL || *editor == '\0') editor = "vi";

    char editor_path[TOOLCHAIN_PATH_LENGTH];
    if (!toolchain_find_executable(editor, editor_path, sizeof(editor_path))) {
        fprintf(
            stderr,
            "neuro: editor is missing or must be a single executable name: %s\n",
            editor
        );
        return 127;
    }
    char *argv[] = {editor_path, (char *)file, NULL};
    return process_run(NULL, argv);
}

static int dispatch(int argc, char **argv) {
    if (argc < 2) {
        return -1;
    }
    const char *command = argv[1];

    if (strcmp(command, "help") == 0) {
        if (argc != 2) return 2;
        print_help(stdout);
        return 0;
    }
    if (strcmp(command, "status") == 0) {
        if (argc != 2) return 2;
        toolchain_print_status();
        return 0;
    }
    if (strcmp(command, "detect") == 0) {
        if (argc > 3) return 2;
        return command_detect(argc == 3 ? argv[2] : ".");
    }
    if (strcmp(command, "init") == 0) {
        if (argc != 4) return 2;
        const enum project_language language = project_language_parse(argv[2]);
        if (language == PROJECT_LANGUAGE_UNKNOWN) {
            fprintf(stderr, "neuro: unsupported language: %s\n", argv[2]);
            return 2;
        }
        const int result = project_init_create(language, argv[3], stdout);
        return result == 0 ? 0 : 1;
    }
    if (strcmp(command, "edit") == 0) {
        if (argc != 3) return 2;
        return command_edit(argv[2]);
    }
    if (strcmp(command, "build") == 0 || strcmp(command, "run") == 0 ||
        strcmp(command, "test") == 0 || strcmp(command, "clean") == 0) {
        if (argc > 3) return 2;
        const char *path = argc == 3 ? argv[2] : ".";
        enum project_language language = PROJECT_LANGUAGE_UNKNOWN;
        const int detect_status = detect_language_for_action(path, &language);
        if (detect_status != 0) return detect_status;

        printf("Project type: %s\n", project_language_name(language));
        if (strcmp(command, "build") == 0) return project_action_build(path, language);
        if (strcmp(command, "run") == 0) return project_action_run(path, language);
        if (strcmp(command, "test") == 0) return project_action_test(path, language);
        return project_action_clean(path, language);
    }

    fprintf(stderr, "neuro: unknown command: %s\n", command);
    return 2;
}

static int interactive_mode(void) {
    puts("NeuroRoute DevCLI v0.4.0 interactive mode");
    puts("Type 'help' for commands or 'quit' to exit.");

    char *line = NULL;
    size_t capacity = 0;
    for (;;) {
        fputs("neuro> ", stdout);
        fflush(stdout);
        const ssize_t length = getline(&line, &capacity, stdin);
        if (length < 0) {
            if (ferror(stdin)) {
                fprintf(stderr, "neuro: input error\n");
                free(line);
                return 1;
            }
            break;
        }

        char *arguments[INTERACTIVE_MAX_ARGUMENTS + 2];
        int count = 1;
        arguments[0] = "neuro";
        char *save_pointer = NULL;
        for (char *token = strtok_r(line, " \t\r\n", &save_pointer);
             token != NULL && count < INTERACTIVE_MAX_ARGUMENTS + 1;
             token = strtok_r(NULL, " \t\r\n", &save_pointer)) {
            arguments[count++] = token;
        }
        arguments[count] = NULL;
        if (count == 1) continue;
        if (strcmp(arguments[1], "quit") == 0 || strcmp(arguments[1], "exit") == 0) {
            break;
        }
        const int status = dispatch(count, arguments);
        if (status == -1) {
            print_help(stdout);
        } else if (status == 2) {
            fputs("neuro: invalid command arguments\n", stderr);
            print_help(stderr);
        } else if (status != 0) {
            fprintf(stderr, "neuro: command exited with status %d\n", status);
        }
    }
    free(line);
    return 0;
}

int devcli_main(int argc, char **argv) {
    if (argc <= 1) return interactive_mode();
    const int status = dispatch(argc, argv);
    if (status == 2) {
        fputs("neuro: invalid command arguments\n", stderr);
        print_help(stderr);
    }
    return status < 0 ? 0 : status;
}
