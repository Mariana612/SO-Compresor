#include "Compressor.h"
#include "../Commons/Codec.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int compress_directory(const char *directory)
{
    DIR *dir = opendir(directory);
    struct dirent *entry;
    int success = 1;

    if (dir == NULL) {
        perror(directory);
        return 0;
    }
    while ((entry = readdir(dir)) != NULL) {
        char path[PATH_MAX];
        struct stat status;
        size_t length;
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
        if (stat(path, &status) != 0 || !S_ISREG(status.st_mode))
            continue;
        length = strlen(path);
        if (length >= 4 && strcmp(path + length - 4, ".txt") == 0 &&
            !common_compress_file(path))
            success = 0;
    }
    closedir(dir);
    return success;
}
