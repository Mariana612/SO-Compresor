#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include "../common/Stats.h"

int compress_directory(const char *directory, const char *archive, RunStats *stats);

#endif
