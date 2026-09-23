#ifndef COMMONS_STATS_H
#define COMMONS_STATS_H

#include <stddef.h>
#include <stdint.h>

/* Estadísticas de una corrida (compresión o descompresión). */
typedef struct {
    size_t files;               // Archivos del directorio o del .huff
    size_t verified;            // Firmas MD5 verificadas (solo al descomprimir)
    uint64_t original_bytes;    // Suma de los tamaños originales
    uint64_t compressed_bytes;  // Tamaño del .huff
    double seconds;             // Tiempo total de la corrida
} RunStats;

/* Salud de la compresión: firmas verificadas / cantidad de archivos, en %. */
double stats_health_percent(const RunStats *stats);

/* Aceleración respecto a la versión serial, en %.
 * Ej. serial 2 s y paralela 1 s -> 100 % (el doble de rápido). */
double stats_speedup_percent(double serial_seconds, double seconds);

/* Imprime en stdout una sola línea "clave=valor" para que la lea la interfaz.
 * mode es 'c' (compresión) o 'd' (descompresión). */
void stats_print(const char *variant, char mode, const RunStats *stats);

#endif
