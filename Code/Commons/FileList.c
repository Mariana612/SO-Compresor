#include "FileList.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static int is_huff(const char *path)
{
    size_t length = strlen(path);
    return length >= 5 && strcmp(path + length - 5, ".huff") == 0;
}

static int add_path(FileList *list, const char *path)
{
    if (list->count == list->capacity) {
        size_t new_capacity = list->capacity == 0 ? 32 : list->capacity * 2;
        char (*new_paths)[PATH_MAX] = realloc(list->paths, new_capacity * sizeof(*list->paths));
        if (new_paths == NULL)
            return 0;
        list->paths = new_paths;
        list->capacity = new_capacity;
    }
    snprintf(list->paths[list->count], PATH_MAX, "%s", path);
    list->count++;
    return 1;
}

int file_list_load(const char *directory, int compressed, FileList *list)
{
    DIR *dir = opendir(directory);
    struct dirent *entry;

    list->paths = NULL;
    list->count = 0;
    list->capacity = 0;
    if (dir == NULL) {
        perror(directory);
        return 0;
    }
    while ((entry = readdir(dir)) != NULL) {
        char path[PATH_MAX];
        struct stat status;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
        if (stat(path, &status) != 0 || !S_ISREG(status.st_mode))
            continue;
        if (is_huff(path) == compressed && !add_path(list, path)) {
            closedir(dir);
            file_list_free(list);
            return 0;
        }
    }
    closedir(dir);
    return 1;
}

void file_list_free(FileList *list)
{
    free(list->paths);
    list->paths = NULL;
    list->count = 0;
    list->capacity = 0;
}
