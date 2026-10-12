#define _POSIX_C_SOURCE 200809L

#include <process_runner.h>

#include <errno.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

int process_run(const char *working_directory, char *const argv[]) {
    if (argv == NULL || argv[0] == NULL || argv[0][0] == '\0') {
        fprintf(stderr, "neuro: invalid child process arguments\n");
        return 126;
    }

    int original_directory = -1;
    if (working_directory != NULL) {
        original_directory = open(".", O_RDONLY);
        if (original_directory < 0) {
            fprintf(stderr, "neuro: cannot save current directory: %s\n", strerror(errno));
            return 126;
        }
        if (chdir(working_directory) != 0) {
            fprintf(
                stderr,
                "neuro: cannot enter '%s': %s\n",
                working_directory,
                strerror(errno)
            );
            close(original_directory);
            return 126;
        }
    }

    fflush(NULL);
    pid_t child = 0;
    const int spawn_result = posix_spawnp(&child, argv[0], NULL, NULL, argv, environ);
    const int spawn_errno = spawn_result;

    if (original_directory >= 0) {
        if (fchdir(original_directory) != 0) {
            fprintf(stderr, "neuro: cannot restore current directory: %s\n", strerror(errno));
            close(original_directory);
            return 126;
        }
        close(original_directory);
    }

    if (spawn_result != 0) {
        fprintf(
            stderr,
            "neuro: failed to start '%s': %s\n",
            argv[0],
            strerror(spawn_errno)
        );
        return spawn_errno == ENOENT ? 127 : 126;
    }

    int status = 0;
    while (waitpid(child, &status, 0) < 0) {
        if (errno == EINTR) {
            continue;
        }
        fprintf(stderr, "neuro: waitpid failed: %s\n", strerror(errno));
        return 126;
    }

    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status)) {
        return 128 + WTERMSIG(status);
    }
    return 126;
}
