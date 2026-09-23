#ifndef FORK_COMPRESSOR_H
#define FORK_COMPRESSOR_H

#include "../common/Stats.h"

int compress_directory(const char *directory, const char *archive, RunStats *stats);

#endif
