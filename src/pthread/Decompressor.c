#include "Decompressor.h"
#include "ThreadPool.h"
#include "../common/Codec.h"
#include <stdlib.h>

// ============================================================
// ESTRUCTURAS DE DATOS
// ============================================================

// Lo que comparten los hilos. La tabla solo se lee, así que no necesita mutex
typedef struct {
    const char *archive;
    const char *output_directory;
    ArchiveEntry *entries;
} DecompressJob;


// ============================================================
// TAREAS DE LOS HILOS
// ============================================================

// Expande un archivo del .huff y verifica su MD5
static int decode_task(size_t index, void *context)
{
    DecompressJob *job = context;
    return codec_decode_entry(job->archive, &job->entries[index], job->output_directory);
}


// ============================================================
// FUNCIONES PRINCIPALES
// ============================================================

// Descomprimir archivo - Reparte los archivos del .huff entre varios hilos
int decompress_archive(const char *archive, const char *output_directory, RunStats *stats)
{
    DecompressJob job;
    size_t count;

    if (!codec_read_header(archive, &job.entries, &count))
        return 0;
    job.archive = archive;
    job.output_directory = output_directory;
    stats->files = count;
    stats->original_bytes = codec_total_size(job.entries, count);

    // Los hilos cuentan las firmas verificadas en la memoria compartida del pool
    stats->verified = thread_pool_run(count, decode_task, &job);

    free(job.entries);
    return 1;
}
