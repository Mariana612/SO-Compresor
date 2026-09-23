#include "ProcessPool.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_CHILDREN 64

// ============================================================
// ESTRUCTURAS DE DATOS
// ============================================================

// Un proceso hijo que está trabajando. El padre guarda su pid para saber
// quién terminó, el extremo de lectura de la pipe para recibir su resultado
// y el índice del elemento que está procesando.
typedef struct {
    pid_t pid;
    int pipe_read;
    size_t index;
} Child;


// ============================================================
// FUNCIONES AUXILIARES
// ============================================================

// Cantidad de hijos - Un hijo por núcleo del procesador (máximo MAX_CHILDREN)
static int get_children_limit(void)
{
    long cores = sysconf(_SC_NPROCESSORS_ONLN);

    if (cores < 1)
        cores = 1;
    if (cores > MAX_CHILDREN)
        cores = MAX_CHILDREN;
    return (int)cores;
}

// Escribir todo - write() puede escribir menos bytes de los pedidos
static int write_full(int fd, const void *buffer, size_t size)
{
    const char *bytes = buffer;
    ssize_t written;

    while (size > 0) {
        written = write(fd, bytes, size);
        if (written < 0 && errno == EINTR)
            continue;
        if (written <= 0)
            return 0;
        bytes += written;
        size -= (size_t)written;
    }
    return 1;
}

// Leer todo - read() puede devolver menos bytes de los pedidos
static int read_full(int fd, void *buffer, size_t size)
{
    char *bytes = buffer;
    ssize_t bytes_read;

    while (size > 0) {
        bytes_read = read(fd, bytes, size);
        if (bytes_read < 0 && errno == EINTR)
            continue;
        if (bytes_read <= 0)
            return 0;   // El hijo terminó sin mandar todo
        bytes += bytes_read;
        size -= (size_t)bytes_read;
    }
    return 1;
}

// Crear hijo - Crea un proceso que ejecuta la tarea y manda el resultado por
// una pipe: primero un int (1 = bien, 0 = error) y después result_size bytes.
static int start_child(Child children[], int *active, size_t index, ProcessTask task,
                       void *context, void *results, size_t result_size)
{
    int fds[2];   // fds[0] = lectura (padre), fds[1] = escritura (hijo)
    pid_t pid;
    int ok, i;

    if (pipe(fds) != 0)
        return 0;

    // Se vacía la salida antes del fork para que el hijo no repita lo que
    // el padre tenía pendiente de imprimir
    fflush(stdout);
    pid = fork();

    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return 0;
    }

    if (pid == 0) {
        // --- Código del HIJO ---
        // El hijo solo escribe: cierra la lectura de su pipe y las de sus hermanos
        char *result = results ? (char *)results + index * result_size : NULL;

        close(fds[0]);
        for (i = 0; i < *active; i++)
            close(children[i].pipe_read);

        ok = task(index, context, result);
        fflush(stdout);

        // Le manda el resultado al padre y termina
        if (!write_full(fds[1], &ok, sizeof(int)) ||
            (result_size > 0 && !write_full(fds[1], result, result_size)))
            ok = 0;
        close(fds[1]);
        _exit(ok ? EXIT_SUCCESS : EXIT_FAILURE);
    }

    // --- Código del PADRE ---
    // El padre solo lee: cierra la escritura y guarda al hijo en la lista
    close(fds[1]);
    children[*active].pid = pid;
    children[*active].pipe_read = fds[0];
    children[*active].index = index;
    (*active)++;
    return 1;
}

// Esperar hijo - Espera al primer hijo que termine y lee su resultado de la pipe
static int wait_for_child(Child children[], int *active, void *results, size_t result_size)
{
    int ok = 0, slot = -1, i;
    pid_t pid;

    // waitpid(-1) espera a cualquier hijo, el que termine primero
    do {
        pid = waitpid(-1, NULL, 0);
    } while (pid < 0 && errno == EINTR);

    // Se busca en qué posición de la lista está ese hijo
    for (i = 0; i < *active; i++) {
        if (children[i].pid == pid)
            slot = i;
    }
    if (slot < 0) {
        // No debería pasar: sin este hijo no hay a quién esperar
        fprintf(stderr, "Error: waitpid devolvió un hijo desconocido\n");
        for (i = 0; i < *active; i++)
            close(children[i].pipe_read);
        *active = 0;
        return 0;
    }

    // Si el hijo no alcanzó a escribir (por ejemplo, se cayó), cuenta como error.
    // El resultado cabe en el buffer de la pipe (ver process_pool_run), así que
    // el hijo pudo escribirlo completo y terminar antes de que el padre lo lea.
    if (!read_full(children[slot].pipe_read, &ok, sizeof(int)))
        ok = 0;
    if (ok && result_size > 0 &&
        !read_full(children[slot].pipe_read,
                   (char *)results + children[slot].index * result_size, result_size))
        ok = 0;
    close(children[slot].pipe_read);

    // Se saca de la lista poniendo en su lugar al último
    children[slot] = children[*active - 1];
    (*active)--;
    return ok;
}


// ============================================================
// FUNCIONES PRINCIPALES
// ============================================================

// Correr pool - Reparte los índices entre varios procesos hijos
size_t process_pool_run(size_t count, ProcessTask task, void *context,
                        void *results, size_t result_size)
{
    Child children[MAX_CHILDREN];
    int active = 0;          // Hijos trabajando en este momento
    int limit = get_children_limit();
    size_t next = 0;         // Siguiente índice que hay que repartir
    size_t succeeded = 0;    // Hijos que avisaron por la pipe que les fue bien

    // El hijo escribe todo antes de que el padre lea: tiene que caber en la pipe
    if (sizeof(int) + result_size > PIPE_BUF) {
        fprintf(stderr, "Error: resultado demasiado grande para la pipe\n");
        return 0;
    }

    // Se sigue mientras queden elementos por repartir o hijos trabajando
    while (next < count || active > 0) {

        // Si hay lugar y quedan elementos, se crea un hijo nuevo
        if (active < limit && next < count) {
            if (start_child(children, &active, next, task, context, results, result_size)) {
                next++;
                continue;
            }
            // No se pudo crear el hijo: si no hay nadie trabajando, se aborta
            // (los elementos sin repartir cuentan como fallidos)
            fprintf(stderr, "Error: no se pudo crear un proceso hijo\n");
            if (active == 0)
                break;
        }

        // No hay lugar para más hijos: se espera a que alguno termine
        if (wait_for_child(children, &active, results, result_size))
            succeeded++;
    }

    return succeeded;
}
