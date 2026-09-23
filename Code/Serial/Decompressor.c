#include "Decompressor.h"
#include "../Commons/Codec.h"

#include <stdlib.h>

int decompress_archive(const char *archive, const char *output_directory)
{
    ArchiveEntry *entries;
    size_t count, index;
    int success = 1;

    if (!codec_read_header(archive, &entries, &count))
        return 0;
    for (index = 0; index < count; index++)
        if (!codec_decode_entry(archive, &entries[index], output_directory))
            success = 0;
    free(entries);
    return success;
}
