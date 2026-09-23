#include "Compressor.h"
#include "ThreadPool.h"
#include "../common/Codec.h"
#include "../common/FileList.h"

// ============================================================
// ESTRUCTURAS DE DATOS
// ============================================================

// Lo que comparten los hilos. La tabla `entries` vive en memoria compartida:
// en la fase 1 cada hilo escribe ahí los metadatos de su archivo.
typedef struct {
    char **paths;
    const char *archive;
    ArchiveEntry *entries;
} CompressJob;


// ============================================================
// TAREAS DE LOS HILOS
// ============================================================

// Fase 1 - MD5, frecuencias y tamaño comprimido, directo en la tabla compartida
static int analyze_task(size_t index, void *context)
{
    CompressJob *job = context;
    return codec_analyze_file(job->paths[index], &job->entries[index]);
}

// Fase 2 - Datos comprimidos en la región del .huff que le toca al archivo
static int encode_task(size_t index, void *context)
{
    CompressJob *job = context;
    return codec_encode_file(job->paths[index], job->archive, &job->entries[index]);
}


// ============================================================
// FUNCIONES PRINCIPALES
// ============================================================

// Comprimir directorio - Reparte los archivos entre varios hilos
int compress_directory(const char *directory, const char *archive, RunStats *stats)
{
    FileList files;
    CompressJob job;
    size_t table_size;
    int success;

    if (!file_list_load(directory, 0, &files))
        return 0;

    // Tabla de metadatos en memoria compartida
    table_size = files.count * sizeof(ArchiveEntry);
    job.paths = files.paths;
    job.archive = archive;
    job.entries = shared_memory_create(table_size);
    if (job.entries == NULL) {
        file_list_free(&files);
        return 0;
    }

    // Fase 1 en paralelo
    success = thread_pool_run(files.count, analyze_task, &job) == files.count;

    // Tabla al inicio del .huff
    if (success) {
        stats->files = files.count;
        stats->original_bytes = codec_total_size(job.entries, files.count);
        codec_assign_offsets(job.entries, files.count);
        success = codec_write_header(archive, job.entries, files.count);
    }

    // Fase 2 en paralelo
    if (success)
        success = thread_pool_run(files.count, encode_task, &job) == files.count;

    shared_memory_destroy(job.entries, table_size);
    file_list_free(&files);
    return success;
}
