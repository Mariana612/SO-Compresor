#ifndef PTHREAD_THREAD_POOL_H
#define PTHREAD_THREAD_POOL_H

#include <stddef.h>

/* Trabajo de un hilo sobre el elemento `index`. Devuelve 1 si salió bien. */
typedef int (*ThreadTask)(size_t index, void *context);

/* Región de memoria compartida creada con mmap. */
void *shared_memory_create(size_t size);
void shared_memory_destroy(void *memory, size_t size);

/* Ejecuta `task` para los índices 0..count-1 repartidos entre varios hilos
 * (uno por núcleo) que toman trabajo de una cola en memoria compartida. */
int thread_pool_run(size_t count, ThreadTask task, void *context);

#endif
