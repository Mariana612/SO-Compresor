#include "Cli.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

// FUNCIONES AUXILIARES ----------------------------------------

// Segundos desde un punto fijo, para medir cuánto tarda la corrida
static double now_seconds(void)
{
    struct timespec time;

    clock_gettime(CLOCK_MONOTONIC, &time);
    return time.tv_sec + time.tv_nsec / 1e9;
}

// Nombre por defecto - "libros/" -> "libros.huff", al lado del directorio
static void default_archive_name(const char *directory, char archive[PATH_MAX])
{
    size_t length = strlen(directory);

    while (length > 1 && directory[length - 1] == '/')
        length--;
    snprintf(archive, PATH_MAX, "%.*s.huff", (int)length, directory);
}

// Preparar salida - Crea el directorio de salida si no existe
static int prepare_output_directory(const char *directory)
{
    struct stat status;

    if (mkdir(directory, 0755) != 0 && errno != EEXIST)
        return 0;
    return stat(directory, &status) == 0 && S_ISDIR(status.st_mode);
}

static int usage(const char *program)
{
    fprintf(stderr, "Uso:\n  %s c <directorio> [archivo.huff]\n"
                    "  %s d <archivo.huff> <directorio_salida>\n",
            program, program);
    return EXIT_FAILURE;
}


// Tamaño del .huff en bytes
static uint64_t archive_size(const char *archive)
{
    struct stat status;

    if (stat(archive, &status) != 0)
        return 0;
    return (uint64_t)status.st_size;
}


// FUNCIONES PRINCIPALES ---------------------------------------

int cli_run(int argc, char *argv[], const char *variant,
            CompressFunction compress, DecompressFunction decompress)
{
    struct stat status;
    char archive[PATH_MAX];
    RunStats stats = {0};
    double start;
    int ok;

    if ((argc == 3 || argc == 4) && strcmp(argv[1], "c") == 0) {
        if (stat(argv[2], &status) != 0 || !S_ISDIR(status.st_mode)) {
            fprintf(stderr, "Error: %s no es un directorio válido.\n", argv[2]);
            return EXIT_FAILURE;
        }
        if (argc == 4)
            snprintf(archive, sizeof(archive), "%s", argv[3]);
        else
            default_archive_name(argv[2], archive);

        start = now_seconds();
        ok = compress(argv[2], archive, &stats);
        stats.seconds = now_seconds() - start;
        if (!ok) {
            fprintf(stderr, "Error: la compresión (%s) falló.\n", variant);
            return EXIT_FAILURE;
        }
        stats.compressed_bytes = archive_size(archive);
        stats_print(variant, 'c', &stats);
        return EXIT_SUCCESS;
    }

    if (argc == 4 && strcmp(argv[1], "d") == 0) {
        if (stat(argv[2], &status) != 0 || !S_ISREG(status.st_mode)) {
            fprintf(stderr, "Error: %s no es un archivo .huff válido.\n", argv[2]);
            return EXIT_FAILURE;
        }
        if (!prepare_output_directory(argv[3])) {
            fprintf(stderr, "Error: directorio de salida inválido: %s\n", argv[3]);
            return EXIT_FAILURE;
        }

        start = now_seconds();
        ok = decompress(argv[2], argv[3], &stats);
        stats.seconds = now_seconds() - start;
        if (!ok) {
            fprintf(stderr, "Error: la descompresión (%s) falló.\n", variant);
            return EXIT_FAILURE;
        }
        // Se imprimen aunque falle alguna firma: la salud muestra cuántas se verificaron
        stats.compressed_bytes = archive_size(argv[2]);
        stats_print(variant, 'd', &stats);
        return stats.verified == stats.files ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    return usage(argv[0]);
}
