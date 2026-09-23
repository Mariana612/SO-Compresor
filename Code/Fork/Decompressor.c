#include "Decompressor.h"
#include "../Commons/Codec.h"
#include "../Commons/FileList.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_CHILDREN 64

// ============================================================
// ESTRUCTURAS DE DATOS
// ============================================================

// Un proceso hijo que está trabajando. El padre guarda su pid para saber
// quién terminó, y el extremo de lectura de la pipe para recibir su resultado.
typedef struct {
    pid_t pid;
    int pipe_read;
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

// Crear hijo - Crea un proceso que descomprime un archivo y avisa el resultado
// por una pipe. Devuelve 0 si no se pudo crear
static int start_child(Child children[], int *active, const char *path,
                       const char *output_directory)
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
        close(fds[0]);
        for (i = 0; i < *active; i++)
            close(children[i].pipe_read);

        ok = common_decompress_file(path, output_directory);
        fflush(stdout);

        // Le manda el resultado (1 = bien, 0 = error) al padre y termina
        if (write(fds[1], &ok, sizeof(int)) != sizeof(int))
            ok = 0;
        close(fds[1]);
        _exit(ok ? EXIT_SUCCESS : EXIT_FAILURE);
    }

    // --- Código del PADRE ---
    // El padre solo lee: cierra la escritura y guarda al hijo en la lista
    close(fds[1]);
    children[*active].pid = pid;
    children[*active].pipe_read = fds[0];
    (*active)++;
    return 1;
}

// Esperar hijo - Espera al primer hijo que termine y lee su resultado de la pipe
static int wait_for_child(Child children[], int *active)
{
    int ok = 0, slot = 0, i;
    pid_t pid;

    // waitpid(-1) espera a cualquier hijo, el que termine primero
    pid = waitpid(-1, NULL, 0);

    // Se busca en qué posición de la lista está ese hijo
    for (i = 0; i < *active; i++) {
        if (children[i].pid == pid)
            slot = i;
    }

    // Si el hijo no alcanzó a escribir (por ejemplo, se cayó), cuenta como error
    if (read(children[slot].pipe_read, &ok, sizeof(int)) != sizeof(int))
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

// Descomprimir directorio - Reparte los archivos entre varios procesos hijos
int decompress_directory(const char *directory, const char *output_directory)
{
    FileList files;
    Child children[MAX_CHILDREN];
    int active = 0;          // Hijos trabajando en este momento
    int success = 1;
    int limit = get_children_limit();
    size_t next = 0;         // Siguiente archivo que hay que repartir

    if (!file_list_load(directory, 1, &files))
        return 0;

    // Se sigue mientras queden archivos por repartir o hijos trabajando
    while (next < files.count || active > 0) {

        // Si hay lugar y quedan archivos, se crea un hijo nuevo
        if (active < limit && next < files.count) {
            if (start_child(children, &active, files.paths[next], output_directory)) {
                next++;
                continue;
            }
            // No se pudo crear el hijo: si no hay nadie trabajando, se aborta
            fprintf(stderr, "Error: no se pudo crear un proceso hijo\n");
            if (active == 0) {
                success = 0;
                break;
            }
        }

        // No hay lugar para más hijos: se espera a que alguno termine
        if (!wait_for_child(children, &active))
            success = 0;
    }

    file_list_free(&files);
    return success;
}
