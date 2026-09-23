#ifndef COMMONS_CLI_H
#define COMMONS_CLI_H

typedef int (*CompressFunction)(const char *directory, const char *archive);
typedef int (*DecompressFunction)(const char *archive, const char *output_directory);

int cli_run(int argc, char *argv[], const char *variant,
            CompressFunction compress, DecompressFunction decompress);

#endif
