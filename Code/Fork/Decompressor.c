#include "Decompressor.h"
#include "ProcessPool.h"
#include "../Commons/Codec.h"
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
int decompress_archive(const char *archive, const char *output_directory)
{
    DecompressJob job;
    size_t count;
    int success;

    if (!codec_read_header(archive, &job.entries, &count))
        return 0;
    job.archive = archive;
    job.output_directory = output_directory;

    success = process_pool_run(count, decode_task, &job, NULL, 0);

    free(job.entries);
    return success;
}
