#define _POSIX_C_SOURCE 200809L

#include <project_init.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <fs_utils.h>

struct expected_project {
    enum project_language language;
    const char *directory;
    const char *file;
};

int main(void) {
    char template[] = "/tmp/neuroroute-init-XXXXXX";
    char *root = mkdtemp(template);
    assert(root != NULL);
    FILE *output = tmpfile();
    assert(output != NULL);

    const struct expected_project projects[] = {
        {PROJECT_LANGUAGE_C, "c", "main.c"},
        {PROJECT_LANGUAGE_CPP, "cpp", "main.cpp"},
        {PROJECT_LANGUAGE_PYTHON, "python", "main.py"},
        {PROJECT_LANGUAGE_RUST, "rust", "Cargo.toml"},
        {PROJECT_LANGUAGE_JAVASCRIPT, "javascript", "main.js"},
        {PROJECT_LANGUAGE_GO, "go", "go.mod"},
        {PROJECT_LANGUAGE_ZIG, "zig", "main.zig"},
        {PROJECT_LANGUAGE_JAVA, "java", "Main.java"}
    };

    for (size_t i = 0; i < sizeof(projects) / sizeof(projects[0]); ++i) {
        char directory[512];
        char file[512];
        assert(fs_join(directory, sizeof(directory), root, projects[i].directory) == 0);
        assert(project_init_create(projects[i].language, directory, output) == 0);
        assert(fs_join(file, sizeof(file), directory, projects[i].file) == 0);
        assert(fs_file_exists(file));
    }

    char protected_directory[512];
    char protected_file[512];
    assert(fs_join(protected_directory, sizeof(protected_directory), root, "protected") == 0);
    assert(fs_mkdirs(protected_directory) == 0);
    assert(fs_join(protected_file, sizeof(protected_file), protected_directory, "keep.txt") == 0);
    assert(fs_write_text_file(protected_file, "keep\n") == 0);
    assert(project_init_create(PROJECT_LANGUAGE_C, protected_directory, output) == -2);

    fclose(output);
    assert(fs_remove_tree(root) == 0);
    puts("project init tests: PASS");
    return 0;
}
