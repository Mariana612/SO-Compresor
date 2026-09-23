#include "Compressor.h"
#include "../Commons/Codec.h"
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_WORKERS 64

typedef struct {
    int ok;
    char filename[PATH_MAX];
} WorkerResult;

static int worker_count(void)
{
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count < 1)
        count = 1;
    return count > MAX_WORKERS ? MAX_WORKERS : (int)count;
}

static int add_path(char (**paths)[PATH_MAX], size_t *count, size_t *capacity,
                    const char *path)
{
    char (*new_paths)[PATH_MAX];
    if (*count == *capacity) {
        size_t new_capacity = *capacity == 0 ? 32 : *capacity * 2;
        new_paths = realloc(*paths, new_capacity * sizeof(**paths));
        if (new_paths == NULL)
            return 0;
        *paths = new_paths;
        *capacity = new_capacity;
    }
    snprintf((*paths)[*count], PATH_MAX, "%s", path);
    (*count)++;
    return 1;
}

int compress_directory(const char *directory)
{
    DIR *dir = opendir(directory);
    struct dirent *entry;
    char (*paths)[PATH_MAX] = NULL;
    size_t count = 0, capacity = 0, next = 0, finished = 0;
    int pipes[MAX_WORKERS][2], active = 0, success = 1;
    pid_t children[MAX_WORKERS];
    int limit = worker_count();

    if (dir == NULL)
        return 0;
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
            !add_path(&paths, &count, &capacity, path)) {
            closedir(dir);
            free(paths);
            return 0;
        }
    }
    closedir(dir);

    while (finished < count) {
        while (active < limit && next < count) {
            if (pipe(pipes[active]) != 0)
                break;
            children[active] = fork();
            if (children[active] == 0) {
                WorkerResult result;
                close(pipes[active][0]);
                result.ok = common_compress_file(paths[next]);
                snprintf(result.filename, sizeof(result.filename), "%s", paths[next]);
                (void)write(pipes[active][1], &result, sizeof(result));
                close(pipes[active][1]);
                _exit(result.ok ? EXIT_SUCCESS : EXIT_FAILURE);
            }
            if (children[active] < 0) {
                close(pipes[active][0]);
                close(pipes[active][1]);
                break;
            }
            close(pipes[active][1]);
            active++;
            next++;
        }
        if (active == 0 && next < count) {
            success = 0;
            break;
        }
        if (active > 0) {
            WorkerResult result;
            ssize_t received = read(pipes[0][0], &result, sizeof(result));
            close(pipes[0][0]);
            waitpid(children[0], NULL, 0);
            if (received != (ssize_t)sizeof(result) || !result.ok)
                success = 0;
            memmove(pipes, pipes + 1, (size_t)(active - 1) * sizeof(pipes[0]));
            memmove(children, children + 1, (size_t)(active - 1) * sizeof(children[0]));
            active--;
            finished++;
        }
    }
    while (active > 0) {
        WorkerResult result;
        ssize_t received = read(pipes[0][0], &result, sizeof(result));
        close(pipes[0][0]);
        waitpid(children[0], NULL, 0);
        if (received != (ssize_t)sizeof(result) || !result.ok)
            success = 0;
        memmove(pipes, pipes + 1, (size_t)(active - 1) * sizeof(pipes[0]));
        memmove(children, children + 1, (size_t)(active - 1) * sizeof(children[0]));
        active--;
    }
    free(paths);
    return success;
}
