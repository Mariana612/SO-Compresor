#include "Compressor.h"
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

// hijo
typedef struct {
    pid_t pid;
    int pipe_read;
} Child;


// ============================================================
// FUNCIONES AUXILIARES
// ============================================================

// Cantidad de hijos - Un hijo por núcleo del procesador
static int get_children_limit(void)
{
    long cores = sysconf(_SC_NPROCESSORS_ONLN);

    if (cores < 1)
        cores = 1;
    if (cores > MAX_CHILDREN)
        cores = MAX_CHILDREN;
    return (int)cores;
}

// Crear hijo - Crea un proceso que comprime un archivo y avisa el resultado

static int start_child(Child children[], int *active, const char *path)
{
    int fds[2];   
    pid_t pid;
    int ok, i;

    if (pipe(fds) != 0)
        return 0;


    fflush(stdout);
    pid = fork();

    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return 0;
    }

    if (pid == 0) {
        // --- Código del HIJO - cierra la lectura de su pipe y las de sus hermanos
        close(fds[0]);
        for (i = 0; i < *active; i++)
            close(children[i].pipe_read);

        ok = common_compress_file(path);
        fflush(stdout);

        // Le manda el resultado (1 = bien, 0 = error) al padre y termina
        if (write(fds[1], &ok, sizeof(int)) != sizeof(int))
            ok = 0;
        close(fds[1]);
        _exit(ok ? EXIT_SUCCESS : EXIT_FAILURE);
    }

    // --- Código PADRE - cierra la escritura y guarda al hijo en la lista
    close(fds[1]); 
    children[*active].pid = pid;
    children[*active].pipe_read = fds[0];
    (*active)++;
    return 1;
}

// Esperar hijo 
static int wait_for_child(Child children[], int *active)
{
    int ok = 0, slot = 0, i;
    pid_t pid;

  
    pid = waitpid(-1, NULL, 0); // ESPERA HIJO

    for (i = 0; i < *active; i++) { // posicion de la lista
        if (children[i].pid == pid)
            slot = i;
    }


    if (read(children[slot].pipe_read, &ok, sizeof(int)) != sizeof(int)) // check
        ok = 0;
    close(children[slot].pipe_read);

    children[slot] = children[*active - 1];
    (*active)--;
    return ok;
}


// ============================================================
// FUNCIONES PRINCIPALES
// ============================================================

// Comprimir directorio - Reparte los archivos entre varios procesos hijos
int compress_directory(const char *directory)
{
    FileList files;
    Child children[MAX_CHILDREN];
    int active = 0;          
    int success = 1;
    int limit = get_children_limit();
    size_t next = 0;         

    if (!file_list_load(directory, 0, &files))
        return 0;


    while (next < files.count || active > 0) {

        // Si hay lugar y quedan archivos
        if (active < limit && next < files.count) {
            if (start_child(children, &active, files.paths[next])) {
                next++;
                continue;
            }
            // No se pudo crear el hijo
            fprintf(stderr, "Error: no se pudo crear un proceso hijo\n");
            if (active == 0) {
                success = 0;
                break;
            }
        }

        // espera a terminar
        if (!wait_for_child(children, &active))
            success = 0;
    }

    file_list_free(&files);
    return success;
}
