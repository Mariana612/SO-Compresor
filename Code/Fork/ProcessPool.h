#ifndef FORK_PROCESS_POOL_H
#define FORK_PROCESS_POOL_H

#include <stddef.h>

/* Trabajo de un hijo: procesa el elemento `index` y deja en `result` los
 * `result_size` bytes que se le mandan al padre por la pipe. Devuelve 1 si salió bien. */
typedef int (*ProcessTask)(size_t index, void *context, void *result);

/* Ejecuta `task` para los índices 0..count-1, cada uno en un proceso hijo
 * (a lo sumo uno por núcleo a la vez). El resultado del hijo i queda en
 * results + i * result_size. `results` puede ser NULL si result_size es 0. */
int process_pool_run(size_t count, ProcessTask task, void *context,
                     void *results, size_t result_size);

#endif
