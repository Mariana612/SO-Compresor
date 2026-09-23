#include "Compressor.h"
#include "ProcessPool.h"
#include "../common/Codec.h"
#include "../common/FileList.h"
#include <stdlib.h>

// ============================================================
// ESTRUCTURAS DE DATOS
// ============================================================

// Lo que necesitan los hijos. Cada hijo recibe una copia al hacer fork()
typedef struct {
    char **paths;
    const char *archive;
    ArchiveEntry *entries;
} CompressJob;


// ============================================================
// TAREAS DE LOS HIJOS
// ============================================================

// Fase 1 - El hijo analiza su archivo y le manda al padre la ArchiveEntry
// completa (MD5, frecuencias y tamaños) por la pipe
static int analyze_task(size_t index, void *context, void *result)
{
    CompressJob *job = context;
    return codec_analyze_file(job->paths[index], result);
}

// Fase 2 - El hijo escribe los datos comprimidos de su archivo en su región del .huff
static int encode_task(size_t index, void *context, void *result)
{
    CompressJob *job = context;
    (void)result;
    return codec_encode_file(job->paths[index], job->archive, &job->entries[index]);
}


// ============================================================
// FUNCIONES PRINCIPALES
// ============================================================

// Comprimir directorio - Reparte los archivos entre varios procesos hijos
int compress_directory(const char *directory, const char *archive, RunStats *stats)
{
    FileList files;
    CompressJob job;
    int success;

    if (!file_list_load(directory, 0, &files))
        return 0;

    job.paths = files.paths;
    job.archive = archive;
    job.entries = calloc(files.count > 0 ? files.count : 1, sizeof(ArchiveEntry));
    if (job.entries == NULL) {
        file_list_free(&files);
        return 0;
    }

    // Fase 1 en paralelo: los metadatos llegan al padre por las pipes
    success = process_pool_run(files.count, analyze_task, &job, job.entries,
                               sizeof(ArchiveEntry)) == files.count;

    // El padre arma la tabla y la escribe al inicio del .huff
    if (success) {
        stats->files = files.count;
        stats->original_bytes = codec_total_size(job.entries, files.count);
        codec_assign_offsets(job.entries, files.count);
        success = codec_write_header(archive, job.entries, files.count);
    }

    // Fase 2 en paralelo: los hijos heredan la tabla con los offsets ya asignados
    if (success)
        success = process_pool_run(files.count, encode_task, &job, NULL, 0) == files.count;

    free(job.entries);
    file_list_free(&files);
    return success;
}
