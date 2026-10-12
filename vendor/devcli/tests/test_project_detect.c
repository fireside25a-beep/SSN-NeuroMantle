#define _POSIX_C_SOURCE 200809L

#include <project_detect.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <fs_utils.h>

int main(void) {
    char template[] = "/tmp/neuroroute-detect-XXXXXX";
    char *directory = mkdtemp(template);
    assert(directory != NULL);

    char path[512];
    assert(fs_join(path, sizeof(path), directory, "main.c") == 0);
    assert(fs_write_text_file(path, "int main(void) { return 0; }\n") == 0);

    struct project_detection detection;
    assert(project_detect_path(directory, &detection) == 0);
    assert(detection.language_count == 1u);
    assert(detection.language == PROJECT_LANGUAGE_C);

    assert(fs_join(path, sizeof(path), directory, "main.py") == 0);
    assert(fs_write_text_file(path, "print('test')\n") == 0);
    assert(project_detect_path(directory, &detection) == 0);
    assert(detection.language_count == 2u);
    assert(detection.language == PROJECT_LANGUAGE_UNKNOWN);

    assert(unlink(path) == 0);
    assert(fs_join(path, sizeof(path), directory, "main.c") == 0);
    assert(unlink(path) == 0);
    assert(fs_join(path, sizeof(path), directory, "Makefile") == 0);
    assert(fs_write_text_file(path, "all:\n\t@true\n") == 0);
    assert(fs_join(path, sizeof(path), directory, "main.cpp") == 0);
    assert(fs_write_text_file(path, "int main() { return 0; }\n") == 0);
    assert(project_detect_path(directory, &detection) == 0);
    assert(detection.language_count == 1u);
    assert(detection.language == PROJECT_LANGUAGE_CPP);

    assert(project_detect_path("/definitely/missing/neuroroute", &detection) == -1);
    assert(fs_remove_tree(directory) == 0);
    puts("project detect tests: PASS");
    return 0;
}
