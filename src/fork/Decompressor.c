#include "Decompressor.h"
#include "ProcessPool.h"
#include "../common/Codec.h"
#include <stdlib.h>

// ============================================================
// ESTRUCTURAS DE DATOS
// ============================================================

// Lo que necesitan los hijos. Cada hijo recibe una copia al hacer fork()
typedef struct {
    const char *archive;
    const char *output_directory;
    ArchiveEntry *entries;
} DecompressJob;


// ============================================================
// TAREAS DE LOS HIJOS
// ============================================================

// El hijo expande un archivo del .huff y verifica su MD5. El resultado
// (1 = verificado, 0 = error) le llega al padre por la pipe
static int decode_task(size_t index, void *context, void *result)
{
    DecompressJob *job = context;
    (void)result;
    return codec_decode_entry(job->archive, &job->entries[index], job->output_directory);
}


// ============================================================
// FUNCIONES PRINCIPALES
// ============================================================

// Descomprimir archivo - Reparte los archivos del .huff entre varios procesos hijos
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

    // Cada hijo avisa por su pipe si verificó la firma; el padre cuenta los que sí
    stats->verified = process_pool_run(count, decode_task, &job, NULL, 0);

    free(job.entries);
    return 1;
}
