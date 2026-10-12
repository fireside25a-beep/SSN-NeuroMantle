#define _POSIX_C_SOURCE 200809L

#include <project_init.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <fs_utils.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static int prepare_directory(const char *directory) {
    if (fs_path_exists(directory)) {
        if (!fs_directory_exists(directory)) {
            fprintf(stderr, "neuro: init destination is not a directory: %s\n", directory);
            return -1;
        }
        const int empty = fs_directory_is_empty(directory);
        if (empty < 0) {
            fprintf(stderr, "neuro: cannot inspect init destination: %s\n", directory);
            return -1;
        }
        if (!empty) {
            fprintf(stderr, "neuro: refusing to overwrite non-empty directory: %s\n", directory);
            return -2;
        }
        return 0;
    }
    if (fs_mkdirs(directory) != 0) {
        fprintf(stderr, "neuro: cannot create directory '%s': %s\n", directory, strerror(errno));
        return -1;
    }
    return 0;
}

static int create_file(
    const char *directory,
    const char *relative_path,
    const char *contents,
    FILE *output
) {
    char full_path[PATH_MAX];
    if (fs_join(full_path, sizeof(full_path), directory, relative_path) != 0) {
        return -1;
    }

    char parent[PATH_MAX];
    const int copied = snprintf(parent, sizeof(parent), "%s", full_path);
    if (copied < 0 || (size_t)copied >= sizeof(parent)) {
        return -1;
    }
    char *separator = strrchr(parent, '/');
    if (separator != NULL) {
        *separator = '\0';
        if (*parent != '\0' && fs_mkdirs(parent) != 0) {
            return -1;
        }
    }

    if (fs_write_text_file(full_path, contents) != 0) {
        fprintf(stderr, "neuro: cannot write '%s'\n", full_path);
        return -1;
    }
    fprintf(output, "  created: %s\n", relative_path);
    return 0;
}

int project_init_create(
    enum project_language language,
    const char *directory,
    FILE *output
) {
    if (directory == NULL || output == NULL || language <= PROJECT_LANGUAGE_UNKNOWN ||
        language >= PROJECT_LANGUAGE_COUNT) {
        return -1;
    }

    const int prepared = prepare_directory(directory);
    if (prepared != 0) {
        return prepared;
    }

    fprintf(output, "Created %s project in %s\n", project_language_name(language), directory);
    switch (language) {
        case PROJECT_LANGUAGE_C:
            return create_file(
                directory,
                "main.c",
                "#include <stdio.h>\n\nint main(void) {\n"
                "    puts(\"Hello from NeuroRoute DevCLI\");\n"
                "    return 0;\n}\n",
                output
            );
        case PROJECT_LANGUAGE_CPP:
            return create_file(
                directory,
                "main.cpp",
                "#include <iostream>\n\nint main() {\n"
                "    std::cout << \"Hello from NeuroRoute DevCLI\\n\";\n"
                "    return 0;\n}\n",
                output
            );
        case PROJECT_LANGUAGE_PYTHON:
            return create_file(
                directory,
                "main.py",
                "print(\"Hello from NeuroRoute DevCLI\")\n",
                output
            );
        case PROJECT_LANGUAGE_RUST:
            if (create_file(
                    directory,
                    "Cargo.toml",
                    "[package]\nname = \"neuroroute-hello\"\nversion = \"0.1.0\"\n"
                    "edition = \"2021\"\n\n[dependencies]\n",
                    output
                ) != 0) return -1;
            return create_file(
                directory,
                "src/main.rs",
                "fn main() {\n    println!(\"Hello from NeuroRoute DevCLI\");\n}\n",
                output
            );
        case PROJECT_LANGUAGE_JAVASCRIPT:
            return create_file(
                directory,
                "main.js",
                "console.log(\"Hello from NeuroRoute DevCLI\");\n",
                output
            );
        case PROJECT_LANGUAGE_GO:
            if (create_file(
                    directory,
                    "go.mod",
                    "module example.com/neuroroute-devcli\n\ngo 1.20\n",
                    output
                ) != 0) return -1;
            return create_file(
                directory,
                "main.go",
                "package main\n\nimport \"fmt\"\n\nfunc main() {\n"
                "\tfmt.Println(\"Hello from NeuroRoute DevCLI\")\n}\n",
                output
            );
        case PROJECT_LANGUAGE_ZIG:
            return create_file(
                directory,
                "main.zig",
                "const std = @import(\"std\");\n\npub fn main() void {\n"
                "    std.debug.print(\"Hello from NeuroRoute DevCLI\\n\", .{});\n}\n",
                output
            );
        case PROJECT_LANGUAGE_JAVA:
            return create_file(
                directory,
                "Main.java",
                "public final class Main {\n"
                "    private Main() {}\n\n"
                "    public static void main(String[] args) {\n"
                "        System.out.println(\"Hello from NeuroRoute DevCLI\");\n"
                "    }\n}\n",
                output
            );
        case PROJECT_LANGUAGE_UNKNOWN:
        case PROJECT_LANGUAGE_COUNT:
            break;
    }
    return -1;
}
