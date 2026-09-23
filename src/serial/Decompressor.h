#ifndef DECOMPRESSOR_H
#define DECOMPRESSOR_H

#include "../common/Stats.h"

int decompress_archive(const char *archive, const char *output_directory,
                       RunStats *stats);

#endif
