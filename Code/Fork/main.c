#include "Compressor.h"
#include "Decompressor.h"
#include "../Commons/Cli.h"

int main(int argc, char *argv[])
{
    return cli_run(argc, argv, "fork", compress_directory, decompress_archive);
}
