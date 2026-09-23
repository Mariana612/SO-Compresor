#ifndef COMMONS_CLI_H
#define COMMONS_CLI_H

#include "Stats.h"

/* Devuelven 1 si la corrida se pudo hacer y dejan en `stats` la cantidad de
 * archivos, sus tamaños originales y (al descomprimir) cuántos se verificaron. */
typedef int (*CompressFunction)(const char *directory, const char *archive, RunStats *stats);
typedef int (*DecompressFunction)(const char *archive, const char *output_directory,
                                  RunStats *stats);

int cli_run(int argc, char *argv[], const char *variant,
            CompressFunction compress, DecompressFunction decompress);

#endif
