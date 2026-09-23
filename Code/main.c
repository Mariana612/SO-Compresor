#include "Compressor.h"
#include "Decompressor.h"

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
        if (stat(argv[2], &status) != 0 || !S_ISREG(status.st_mode) ||
            stat(argv[3], &status) != 0 || !S_ISDIR(status.st_mode)) {
            fprintf(stderr, "Error: archivo comprimido o directorio inválido.\n");
            return EXIT_FAILURE;
        }
        if (!decompress_file(argv[2], argv[3])) {
            fprintf(stderr, "\nLa descompresión NO fue verificada correctamente.\n");
            return EXIT_FAILURE;
        }
        printf("\nDescompresión y verificación terminadas correctamente.\n");
        return EXIT_SUCCESS;
    }

    fprintf(stderr, "Uso:\n  %s c <directorio>\n  %s d <archivo.huff> <directorio_salida>\n",
            argv[0], argv[0]);
    return EXIT_FAILURE;
}