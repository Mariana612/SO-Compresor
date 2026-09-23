#include "ThreadPool.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#define MAX_THREADS 64

// ============================================================
// ESTRUCTURAS DE DATOS
// ============================================================

// Datos que comparten todos los hilos. Funciona como una fila de trabajo:
// cada hilo toma el siguiente índice pendiente hasta que no quede ninguno.
// Se guarda en memoria compartida creada con mmap, y el mutex evita que dos
// hilos modifiquen estos datos al mismo tiempo.
typedef struct {
    pthread_mutex_t mutex;
    size_t next_index;    // Siguiente elemento a procesar
    size_t total;         // Cantidad de elementos
    size_t succeeded;     // Elementos que salieron bien
    ThreadTask task;
    void *context;
} SharedData;


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

// Tomar elemento - Saca el siguiente índice de la fila. Devuelve 0 si ya no quedan
static int take_next(SharedData *shared, size_t *index)
{
    int found = 0;

    pthread_mutex_lock(&shared->mutex);
    if (shared->next_index < shared->total) {
        *index = shared->next_index;
        shared->next_index++;
        found = 1;
    }
    pthread_mutex_unlock(&shared->mutex);

    return found;
}

// Marcar éxito - Suma en la memoria compartida un elemento que salió bien
static void mark_success(SharedData *shared)
{
    pthread_mutex_lock(&shared->mutex);
    shared->succeeded++;
    pthread_mutex_unlock(&shared->mutex);
}

// Trabajo del hilo - Cada hilo procesa elementos hasta que la fila se vacía
static void *worker(void *arg)
{
    SharedData *shared = arg;
    size_t index;

    while (take_next(shared, &index)) {
        if (shared->task(index, shared->context))
            mark_success(shared);
    }
    return NULL;
}


// ============================================================
// FUNCIONES PRINCIPALES
// ============================================================

// Crear memoria compartida - Región anónima compartida (MAP_SHARED) con mmap
void *shared_memory_create(size_t size)
{
    void *memory = mmap(NULL, size > 0 ? size : 1, PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS, -1, 0);

    if (memory == MAP_FAILED) {
        fprintf(stderr, "Error: no se pudo crear la memoria compartida\n");
        return NULL;
    }
    return memory;
}

// Liberar memoria compartida
void shared_memory_destroy(void *memory, size_t size)
{
    if (memory != NULL)
        munmap(memory, size > 0 ? size : 1);
}

// Correr pool - Reparte los índices entre varios hilos
size_t thread_pool_run(size_t count, ThreadTask task, void *context)
{
    SharedData *shared;
    pthread_t threads[MAX_THREADS];
    int thread_count, created = 0, i;
    size_t result;

    // Fila de trabajo en memoria compartida
    shared = shared_memory_create(sizeof(SharedData));
    if (shared == NULL)
        return 0;
    pthread_mutex_init(&shared->mutex, NULL);
    shared->next_index = 0;
    shared->total = count;
    shared->succeeded = 0;
    shared->task = task;
    shared->context = context;

    // Crear los hilos (no más hilos que elementos)
    thread_count = get_thread_count();
    if ((size_t)thread_count > count)
        thread_count = (int)count;

    for (i = 0; i < thread_count; i++) {
        if (pthread_create(&threads[i], NULL, worker, shared) != 0) {
            fprintf(stderr, "Error: no se pudo crear el hilo %d\n", i);
            break;
        }
        created++;
    }

    // Esperar a que todos los hilos terminen
    for (i = 0; i < created; i++)
        pthread_join(threads[i], NULL);

    // Guardar el resultado y liberar todo. Si no se creó ningún hilo, los
    // elementos quedaron sin procesar y no suman como exitosos
    result = shared->succeeded;
    pthread_mutex_destroy(&shared->mutex);
    shared_memory_destroy(shared, sizeof(SharedData));
    return result;
}
