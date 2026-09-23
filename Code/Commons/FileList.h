#ifndef COMMONS_FILE_LIST_H
#define COMMONS_FILE_LIST_H

#include <stddef.h>

typedef struct {
    char **paths;   // Arreglo de rutas, ej. "libros/pg11.txt"
    size_t count;   // Cantidad de rutas guardadas
} FileList;


int file_list_load(const char *directory, int compressed, FileList *list);
void file_list_free(FileList *list);

#endif
