#ifndef COMPRESSOR_H
#define COMPRESSOR_H

#include "../Commons/Stats.h"

int compress_directory(const char *directory, const char *archive, RunStats *stats);

#endif
