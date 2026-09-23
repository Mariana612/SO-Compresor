#include "Compressor.h"
#include "../Commons/Codec.h"
#include "../Commons/FileList.h"
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define MAX_THREADS 64

/* Estado compartido por todos los hilos: la cola de trabajo (next/total) y el
 * resultado global. Se ubica en una región mmap(MAP_SHARED) para hacer explícita
 * la memoria compartida; el mutex serializa el acceso. */
typedef struct {
    pthread_mutex_t mutex;
    size_t next;
    size_t total;
    int success;
} SharedState;

typedef struct {
    SharedState *state;
    char **paths;
} WorkerArgs;

static int thread_count(void)
{
    long count = sysconf(_SC_NPROCESSORS_ONLN);
    if (count < 1)
        count = 1;
    return count > MAX_THREADS ? MAX_THREADS : (int)count;
}

static void *compress_worker(void *argument)
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

        ok = common_compress_file(worker->paths[index]);
        if (!ok) {
            pthread_mutex_lock(&worker->state->mutex);
            worker->state->success = 0;
            pthread_mutex_unlock(&worker->state->mutex);
        }
    }
    return NULL;
}

int compress_directory(const char *directory)
{
    FileList files;
    SharedState *state;
    WorkerArgs arguments;
    pthread_t threads[MAX_THREADS];
    int created = 0, index, result = 0;

    if (!file_list_load(directory, 0, &files))
        return 0;

    state = mmap(NULL, sizeof(*state), PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (state == MAP_FAILED) {
        file_list_free(&files);
        return 0;
    }
    pthread_mutex_init(&state->mutex, NULL);
    state->next = 0;
    state->total = files.count;
    state->success = 1;
    arguments.state = state;
    arguments.paths = files.paths;

    for (index = 0; index < thread_count() && (size_t)index < files.count; index++) {
        if (pthread_create(&threads[created], NULL, compress_worker, &arguments) != 0) {
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
    file_list_free(&files);
    return result;
}
