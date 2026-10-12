#define _POSIX_C_SOURCE 200809L

#include <fs_utils.h>

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

int fs_join(char *out, size_t out_size, const char *left, const char *right) {
    if (out == NULL || out_size == 0 || left == NULL || right == NULL) {
        return -1;
    }
    const size_t left_length = strlen(left);
    const int needs_separator = left_length > 0 && left[left_length - 1] != '/';
    const int written = snprintf(
        out,
        out_size,
        needs_separator ? "%s/%s" : "%s%s",
        left,
        right
    );
    return written < 0 || (size_t)written >= out_size ? -1 : 0;
}

int fs_path_exists(const char *path) {
    struct stat status;
    return path != NULL && lstat(path, &status) == 0;
}

int fs_file_exists(const char *path) {
    struct stat status;
    return path != NULL && stat(path, &status) == 0 && S_ISREG(status.st_mode);
}

int fs_directory_exists(const char *path) {
    struct stat status;
    return path != NULL && stat(path, &status) == 0 && S_ISDIR(status.st_mode);
}

int fs_directory_is_empty(const char *path) {
    DIR *directory = opendir(path);
    if (directory == NULL) {
        return -1;
    }

    int empty = 1;
    struct dirent *entry = NULL;
    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            empty = 0;
            break;
        }
    }
    closedir(directory);
    return empty;
}

int fs_mkdirs(const char *path) {
    if (path == NULL || *path == '\0') {
        return -1;
    }

    char buffer[PATH_MAX];
    const int copied = snprintf(buffer, sizeof(buffer), "%s", path);
    if (copied < 0 || (size_t)copied >= sizeof(buffer)) {
        return -1;
    }

    size_t length = strlen(buffer);
    while (length > 1 && buffer[length - 1] == '/') {
        buffer[--length] = '\0';
    }

    for (char *cursor = buffer + 1; *cursor != '\0'; ++cursor) {
        if (*cursor != '/') {
            continue;
        }
        *cursor = '\0';
        if (mkdir(buffer, 0777) != 0 && errno != EEXIST) {
            return -1;
        }
        *cursor = '/';
    }

    if (mkdir(buffer, 0777) != 0 && errno != EEXIST) {
        return -1;
    }
    return fs_directory_exists(buffer) ? 0 : -1;
}

int fs_write_text_file(const char *path, const char *text) {
    if (path == NULL || text == NULL) {
        return -1;
    }
    FILE *file = fopen(path, "wb");
    if (file == NULL) {
        return -1;
    }
    const size_t length = strlen(text);
    const size_t written = fwrite(text, 1, length, file);
    const int close_result = fclose(file);
    return written == length && close_result == 0 ? 0 : -1;
}

static int remove_tree_internal(const char *path) {
    struct stat status;
    if (lstat(path, &status) != 0) {
        return errno == ENOENT ? 0 : -1;
    }

    if (!S_ISDIR(status.st_mode) || S_ISLNK(status.st_mode)) {
        return unlink(path) == 0 ? 0 : -1;
    }

    DIR *directory = opendir(path);
    if (directory == NULL) {
        return -1;
    }

    int result = 0;
    struct dirent *entry = NULL;
    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char child[PATH_MAX];
        if (fs_join(child, sizeof(child), path, entry->d_name) != 0) {
            result = -1;
            break;
        }
        if (remove_tree_internal(child) != 0) {
            result = -1;
            break;
        }
    }
    closedir(directory);

    if (result == 0 && rmdir(path) != 0) {
        result = -1;
    }
    return result;
}

int fs_remove_tree(const char *path) {
    if (path == NULL || *path == '\0' || strcmp(path, "/") == 0) {
        return -1;
    }
    return remove_tree_internal(path);
}

static int has_suffix(const char *text, const char *suffix) {
    const size_t text_length = strlen(text);
    const size_t suffix_length = strlen(suffix);
    return text_length >= suffix_length &&
        strcmp(text + text_length - suffix_length, suffix) == 0;
}

static int remove_python_cache_internal(const char *path) {
    DIR *directory = opendir(path);
    if (directory == NULL) {
        return errno == ENOENT ? 0 : -1;
    }

    int result = 0;
    struct dirent *entry = NULL;
    while ((entry = readdir(directory)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        char child[PATH_MAX];
        if (fs_join(child, sizeof(child), path, entry->d_name) != 0) {
            result = -1;
            break;
        }

        struct stat status;
        if (lstat(child, &status) != 0) {
            result = -1;
            break;
        }
        if (S_ISDIR(status.st_mode) && !S_ISLNK(status.st_mode)) {
            if (strcmp(entry->d_name, "__pycache__") == 0) {
                if (fs_remove_tree(child) != 0) {
                    result = -1;
                    break;
                }
            } else if (remove_python_cache_internal(child) != 0) {
                result = -1;
                break;
            }
        } else if (S_ISREG(status.st_mode) && has_suffix(entry->d_name, ".pyc")) {
            if (unlink(child) != 0) {
                result = -1;
                break;
            }
        }
    }
    closedir(directory);
    return result;
}

int fs_remove_python_cache(const char *path) {
    if (path == NULL || !fs_directory_exists(path)) {
        return -1;
    }
    return remove_python_cache_internal(path);
}
