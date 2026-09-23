#include "Stats.h"

#include <inttypes.h>
#include <stdio.h>

// FUNCIONES PRINCIPALES ---------------------------------------

// Salud - Porcentaje de archivos cuya firma MD5 se verificó
double stats_health_percent(const RunStats *stats)
{
    if (stats->files == 0)
        return 100.0;   // Nada que verificar, nada falló
    return 100.0 * (double)stats->verified / (double)stats->files;
}

// Aceleración - Cuánto más rápido fue que la serial: (Ts / T - 1) x 100
double stats_speedup_percent(double serial_seconds, double seconds)
{
    if (serial_seconds <= 0 || seconds <= 0)
        return 0.0;
    return (serial_seconds / seconds - 1.0) * 100.0;
}

// Imprimir - Una línea con todas las estadísticas de la corrida
//   compresión:    variante=fork operacion=c archivos=100 original=... comprimido=... tiempo=...
//   descompresión: ... archivos=100 verificados=100 salud=100.00 original=... comprimido=... tiempo=...
void stats_print(const char *variant, char mode, const RunStats *stats)
{
    printf("variante=%s operacion=%c archivos=%zu", variant, mode, stats->files);
    if (mode == 'd')
        printf(" verificados=%zu salud=%.2f", stats->verified, stats_health_percent(stats));
    printf(" original=%" PRIu64 " comprimido=%" PRIu64 " tiempo=%.6f\n",
           stats->original_bytes, stats->compressed_bytes, stats->seconds);
    fflush(stdout);
}
