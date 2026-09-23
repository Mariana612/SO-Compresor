#include "Decompressor.h"
#include "../Commons/Codec.h"

#include <stdlib.h>

int decompress_archive(const char *archive, const char *output_directory, RunStats *stats)
{
    ArchiveEntry *entries;
    size_t count, index;

    if (!codec_read_header(archive, &entries, &count))
        return 0;
    stats->files = count;
    stats->original_bytes = codec_total_size(entries, count);

    // Se descomprimen todos aunque alguno falle, contando las firmas verificadas
    for (index = 0; index < count; index++)
        if (codec_decode_entry(archive, &entries[index], output_directory))
            stats->verified++;

    free(entries);
    return 1;
}
