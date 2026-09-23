#include "FileList.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

// FUNCIONES AUXILIARES ----------------------------------------

// Es .huff - Revisa si el nombre del archivo termina en ".huff"
static int is_huff(const char *path)
{
    size_t length = strlen(path);

    if (length < 5)
        return 0;
    return strcmp(path + length - 5, ".huff") == 0;
}

// Agregar ruta - Agranda el arreglo en una posición y guarda una copia de la ruta
static int add_path(FileList *list, const char *path)
{
    char **new_paths = realloc(list->paths, (list->count + 1) * sizeof(char *));
    if (new_paths == NULL)
        return 0;
    list->paths = new_paths;

    list->paths[list->count] = malloc(strlen(path) + 1);
    if (list->paths[list->count] == NULL)
        return 0;
    strcpy(list->paths[list->count], path);

    list->count++;
    return 1;
}


// FUNCIONES PRINCIPALES ---------------------------------------

// Cargar lista - Recorre el directorio y guarda los archivos que sirven
int file_list_load(const char *directory, int compressed, FileList *list)
{
    DIR *dir;
    struct dirent *entry;
    char path[PATH_MAX];
    struct stat info;

    list->paths = NULL;
    list->count = 0;

    dir = opendir(directory);
    if (dir == NULL) {
        fprintf(stderr, "Error: no se pudo abrir el directorio %s\n", directory);
        return 0;
    }

    while ((entry = readdir(dir)) != NULL) {
        // "." es el directorio actual y ".." el anterior: se saltan
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Ruta completa: "directorio/nombre"
        snprintf(path, PATH_MAX, "%s/%s", directory, entry->d_name);

        // Solo archivos normales (no carpetas ni otras cosas)
        if (stat(path, &info) != 0 || !S_ISREG(info.st_mode))
            continue;

        // Al comprimir se saltan los .huff; al descomprimir solo se usan los .huff
        if (compressed == 1 && !is_huff(path))
            continue;
        if (compressed == 0 && is_huff(path))
            continue;

        if (!add_path(list, path)) {
            fprintf(stderr, "Error: no hay memoria para la lista de archivos\n");
            closedir(dir);
            file_list_free(list);
            return 0;
        }
    }

    closedir(dir);
    return 1;
}

// Liberar lista - Libera cada ruta y después el arreglo
void file_list_free(FileList *list)
{
    size_t i;

    for (i = 0; i < list->count; i++)
        free(list->paths[i]);
    free(list->paths);

    list->paths = NULL;
    list->count = 0;
}
