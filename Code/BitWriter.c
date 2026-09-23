#include "BitWriter.h"

void bitwriter_init(BitWriter *writer, FILE *file)
{
    writer->file = file;
    writer->buffer = 0;
    writer->bits = 0;
}

void write_bit(BitWriter *writer, int bit)
{
    writer->buffer <<= 1;

    if (bit)
        writer->buffer |= 1;

    writer->bits++;

    if (writer->bits == 8) {
        fputc(writer->buffer, writer->file);
        writer->buffer = 0;
        writer->bits = 0;
    }
}

void write_code(
        BitWriter *writer,
        const unsigned char bits[],
        int length)
{
    for (int i = 0; i < length; i++)
        write_bit(writer, bits[i]);
}

void bitwriter_flush(BitWriter *writer)
{
    if (writer->bits > 0) {
        writer->buffer <<= 8 - writer->bits;
        fputc(writer->buffer, writer->file);
        writer->buffer = 0;
        writer->bits = 0;
    }
}
