#include "Compressor.h"
#include "Decompressor.h"
#include "../Commons/Codec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

int main(int argc, char *argv[])
{
    struct stat status;

    if (argc == 3 && strcmp(argv[1], "c") == 0) {
        if (stat(argv[2], &status) != 0 || !S_ISDIR(status.st_mode)) {
            fprintf(stderr, "Error: %s no es un directorio válido.\n", argv[2]);
            return EXIT_FAILURE;
        }
        if (!compress_directory(argv[2]))
            return EXIT_FAILURE;
        printf("\nProceso de compresión terminado.\n");
        return EXIT_SUCCESS;
    }

    if (argc == 4 && strcmp(argv[1], "d") == 0) {
        struct stat input_status;
        int ok;
        if (stat(argv[3], &status) != 0 || !S_ISDIR(status.st_mode) ||
            stat(argv[2], &input_status) != 0 ||
            (!S_ISDIR(input_status.st_mode) && !S_ISREG(input_status.st_mode))) {
            fprintf(stderr, "Error: archivo/directorio de comprimidos o de salida inválido.\n");
            return EXIT_FAILURE;
        }
        ok = S_ISREG(input_status.st_mode) ? common_decompress_file(argv[2], argv[3])
                                            : decompress_directory(argv[2], argv[3]);
        if (!ok) {
            fprintf(stderr, "\nLa descompresión NO fue verificada correctamente.\n");
            return EXIT_FAILURE;
        }
        printf("\nDescompresión y verificación terminadas correctamente.\n");
        return EXIT_SUCCESS;
    }

    fprintf(stderr, "Uso:\n  %s c <directorio>\n"
                    "  %s d <directorio_huff | archivo.huff> <directorio_salida>\n",
            argv[0], argv[0]);
    return EXIT_FAILURE;
}