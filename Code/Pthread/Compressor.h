#ifndef PTHREAD_COMPRESSOR_H
#define PTHREAD_COMPRESSOR_H

#include "../Commons/Stats.h"

int compress_directory(const char *directory, const char *archive, RunStats *stats);

#endif
