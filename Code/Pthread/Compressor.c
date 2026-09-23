#include "Compressor.h"
#include "../Commons/Codec.h"
#include "../Commons/FileList.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define MAX_THREADS 64

// ============================================================
// ESTRUCTURAS DE DATOS
// ============================================================

// Mutex
typedef struct {
    pthread_mutex_t mutex;
    size_t next_file;     // Posición del siguiente archivo a procesar
    size_t total_files;   // Cantidad de archivos en la lista
    int success;          // Queda en 0 si algún archivo falla
} SharedData;

// Hilo
typedef struct {
    SharedData *shared;
    char **paths;
} ThreadArgs;


// ============================================================
// FUNCIONES AUXILIARES
// ============================================================

// Cantidad de hilos - Un hilo por núcleo del procesador (máximo MAX_THREADS)
static int get_thread_count(void)
{
    long cores = sysconf(_SC_NPROCESSORS_ONLN);

    if (cores < 1)
        cores = 1;
    if (cores > MAX_THREADS)
        cores = MAX_THREADS;
    return (int)cores;
}

// Tomar archivo - Saca el siguiente archivo de la fila. 
static int take_next_file(SharedData *shared, size_t *index)
{
    int found = 0;

    pthread_mutex_lock(&shared->mutex);
    if (shared->next_file < shared->total_files) {
        *index = shared->next_file;
        shared->next_file++;
        found = 1;
    }
    pthread_mutex_unlock(&shared->mutex);

    return found;
}

// Marcar error - Anota en la memoria compartida que algún archivo falló
static void mark_error(SharedData *shared)
{
    pthread_mutex_lock(&shared->mutex);
    shared->success = 0;
    pthread_mutex_unlock(&shared->mutex);
}


// ============================================================
// FUNCIONES PRINCIPALES
// ============================================================

// Trabajo del hilo - Cada hilo comprime archivos hasta que la fila se vacía
static void *compress_worker(void *arg)
{
    ThreadArgs *args = (ThreadArgs *)arg;
    size_t index;

    while (take_next_file(args->shared, &index)) {
        if (!common_compress_file(args->paths[index]))
            mark_error(args->shared);
    }
    return NULL;
}

// Comprimir directorio - Reparte los archivos entre varios hilos
int compress_directory(const char *directory)
{
    FileList files;
    SharedData *shared;
    ThreadArgs args;
    pthread_t threads[MAX_THREADS];
    int thread_count, created = 0, i, result;

    //archivos del directorio
    if (!file_list_load(directory, 0, &files))
        return 0;

    // Mutex
    shared = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared == MAP_FAILED) {
        fprintf(stderr, "Error: no se pudo crear la memoria compartida\n");
        file_list_free(&files);
        return 0;
    }
    pthread_mutex_init(&shared->mutex, NULL);
    shared->next_file = 0;
    shared->total_files = files.count;
    shared->success = 1;

    args.shared = shared;
    args.paths = files.paths;

    // Hilos
    thread_count = get_thread_count();
    if ((size_t)thread_count > files.count)
        thread_count = (int)files.count;

    for (i = 0; i < thread_count; i++) {
        if (pthread_create(&threads[i], NULL, compress_worker, &args) != 0) {
            fprintf(stderr, "Error: no se pudo crear el hilo %d\n", i);
            mark_error(shared);
            break;
        }
        created++;
    }

    // Espera a que todos los hilos terminen
    for (i = 0; i < created; i++)
        pthread_join(threads[i], NULL);

    // Fin
    result = shared->success;
    pthread_mutex_destroy(&shared->mutex);
    munmap(shared, sizeof(SharedData));
    file_list_free(&files);
    return result;
}
