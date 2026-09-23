#ifndef FORK_DECOMPRESSOR_H
#define FORK_DECOMPRESSOR_H

#include "../common/Stats.h"

int decompress_archive(const char *archive, const char *output_directory,
                       RunStats *stats);

#endif
