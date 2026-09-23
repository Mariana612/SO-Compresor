#include "Decompressor.h"
#include "../Commons/Codec.h"
#include <dirent.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_THREADS 64

typedef struct {
    pthread_mutex_t mutex;
    size_t next;
    size_t total;
    int success;
} SharedState;

typedef struct {
    SharedState *state;
    char (*paths)[PATH_MAX];
    const char *output_directory;
} WorkerArgs;

static int thread_count(void)
{
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count < 1)
        count = 1;
    return count > MAX_THREADS ? MAX_THREADS : (int)count;
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

static void *decompress_worker(void *argument)
{
    WorkerArgs *worker = argument;
    while (1) {
        size_t index;
        int ok;
        pthread_mutex_lock(&worker->state->mutex);
        if (worker->state->next >= worker->state->total) {
            pthread_mutex_unlock(&worker->state->mutex);
            break;
        }
        index = worker->state->next++;
        pthread_mutex_unlock(&worker->state->mutex);

        ok = common_decompress_file(worker->paths[index], worker->output_directory);
        if (!ok) {
            pthread_mutex_lock(&worker->state->mutex);
            worker->state->success = 0;
            pthread_mutex_unlock(&worker->state->mutex);
        }
    }
    return NULL;
}

int decompress_directory(const char *directory, const char *output_directory)
{
    DIR *dir = opendir(directory);
    struct dirent *entry;
    char (*paths)[PATH_MAX] = NULL;
    size_t count = 0, capacity = 0;
    SharedState *state;
    WorkerArgs arguments;
    pthread_t threads[MAX_THREADS];
    int created = 0, index, result = 0;

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
        if (length >= 5 && strcmp(path + length - 5, ".huff") == 0 &&
            !add_path(&paths, &count, &capacity, path)) {
            closedir(dir);
            free(paths);
            return 0;
        }
    }
    closedir(dir);

    state = mmap(NULL, sizeof(*state), PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (state == MAP_FAILED) {
        free(paths);
        return 0;
    }
    pthread_mutex_init(&state->mutex, NULL);
    state->next = 0;
    state->total = count;
    state->success = 1;
    arguments.state = state;
    arguments.paths = paths;
    arguments.output_directory = output_directory;

    for (index = 0; index < thread_count() && (size_t)index < count; index++) {
        if (pthread_create(&threads[created], NULL, decompress_worker, &arguments) != 0) {
            pthread_mutex_lock(&state->mutex);
            state->success = 0;
            pthread_mutex_unlock(&state->mutex);
            break;
        }
        created++;
    }
    for (index = 0; index < created; index++)
        pthread_join(threads[index], NULL);
    result = state->success;
    pthread_mutex_destroy(&state->mutex);
    munmap(state, sizeof(*state));
    free(paths);
    return result;
}
