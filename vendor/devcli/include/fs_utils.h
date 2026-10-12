#ifndef NEUROROUTE_FS_UTILS_H
#define NEUROROUTE_FS_UTILS_H

#include <stddef.h>

int fs_path_exists(const char *path);
int fs_file_exists(const char *path);
int fs_directory_exists(const char *path);
int fs_directory_is_empty(const char *path);
int fs_mkdirs(const char *path);
int fs_write_text_file(const char *path, const char *text);
int fs_remove_tree(const char *path);
int fs_remove_python_cache(const char *path);
int fs_join(char *out, size_t out_size, const char *left, const char *right);

#endif
