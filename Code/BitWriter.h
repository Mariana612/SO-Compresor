#ifndef BITWRITER_H
#define BITWRITER_H

#include <stdio.h>

typedef struct {
    FILE *file;
    unsigned char buffer;
    int bits;
} BitWriter;

void bitwriter_init(BitWriter *writer, FILE *file);
void write_bit(BitWriter *writer, int bit);
void write_code(BitWriter *writer, const unsigned char bits[], int length);
void bitwriter_flush(BitWriter *writer);

#endif
