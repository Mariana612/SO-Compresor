#ifndef COMMONS_CODEC_H
#define COMMONS_CODEC_H

#include "HuffmanTree.h"
#include "MD5Utils.h"

#include <stddef.h>
#include <stdint.h>

#define ARCHIVE_NAME_MAX 256   // Largo máximo del nombre de un archivo (con el '\0')

/* Metadatos de un archivo guardado dentro del .huff. Con las frecuencias se
 * reconstruye el árbol de Huffman y con el MD5 se verifica la descompresión. */
typedef struct {
    char name[ARCHIVE_NAME_MAX];                  // Nombre original, sin carpeta
    uint64_t original_size;                       // Bytes del archivo original
    uint64_t compressed_size;                     // Bytes de sus datos comprimidos
    uint64_t data_offset;                         // Dónde empiezan sus datos en el .huff
    unsigned char md5[MD5_DIGEST_LENGTH];         // Firma MD5 del original
    uint64_t frequencies[HUFFMAN_SYMBOLS];        // Tabla de frecuencias
} ArchiveEntry;

// Compresión
int codec_analyze_file(const char *path, ArchiveEntry *entry);
void codec_assign_offsets(ArchiveEntry entries[], size_t count);
int codec_write_header(const char *archive, const ArchiveEntry entries[], size_t count);
int codec_encode_file(const char *path, const char *archive, const ArchiveEntry *entry);
uint64_t codec_total_size(const ArchiveEntry entries[], size_t count);

// Descompresión (codec_decode_entry devuelve 1 solo si la firma MD5 se verificó)
int codec_read_header(const char *archive, ArchiveEntry **entries, size_t *count);
int codec_decode_entry(const char *archive, const ArchiveEntry *entry, const char *output_directory);

#endif
