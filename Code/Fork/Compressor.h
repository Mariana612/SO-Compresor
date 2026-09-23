#ifndef FORK_COMPRESSOR_H
#define FORK_COMPRESSOR_H

#include "../Commons/Stats.h"

int compress_directory(const char *directory, const char *archive, RunStats *stats);

#endif
