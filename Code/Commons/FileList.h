#ifndef COMMONS_FILE_LIST_H
#define COMMONS_FILE_LIST_H

#include <limits.h>
#include <stddef.h>

typedef struct {
    char (*paths)[PATH_MAX];
    size_t count;
    size_t capacity;
} FileList;

/* compressed = 1: solo archivos .huff; compressed = 0: todo archivo regular excepto .huff */
int file_list_load(const char *directory, int compressed, FileList *list);
void file_list_free(FileList *list);

#endif
