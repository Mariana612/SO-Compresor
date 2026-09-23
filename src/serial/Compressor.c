#include "Compressor.h"
#include "../common/Codec.h"
#include "../common/FileList.h"

#include <stdlib.h>

int compress_directory(const char *directory, const char *archive, RunStats *stats)
{
    FileList files;
    ArchiveEntry *entries;
    size_t index;
    int success = 1;

    if (!file_list_load(directory, 0, &files))
        return 0;
    entries = calloc(files.count > 0 ? files.count : 1, sizeof(ArchiveEntry));
    if (entries == NULL) {
        file_list_free(&files);
        return 0;
    }

    // Fase 1: MD5, frecuencias y tamaño comprimido de cada archivo
    for (index = 0; index < files.count && success; index++)
        if (!codec_analyze_file(files.paths[index], &entries[index]))
            success = 0;

    // Tabla de metadatos al inicio del .huff
    if (success) {
        stats->files = files.count;
        stats->original_bytes = codec_total_size(entries, files.count);
        codec_assign_offsets(entries, files.count);
        success = codec_write_header(archive, entries, files.count);
    }

    // Fase 2: datos comprimidos de cada archivo en su offset
    for (index = 0; index < files.count && success; index++)
        if (!codec_encode_file(files.paths[index], archive, &entries[index]))
            success = 0;

    free(entries);
    file_list_free(&files);
    return success;
}
