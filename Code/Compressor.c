#include "Compressor.h"

#include "HuffmanTree.h"
#include "MD5Utils.h"

#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define BUFFER_SIZE (1024 * 1024)
#define MAGIC "HUF2"

typedef struct {
    FILE *file;
    unsigned char buffer;
    int bits;
} BitWriter;

static void bitwriter_init(BitWriter *writer, FILE *file)
{
    writer->file = file;
    writer->buffer = 0;
    writer->bits = 0;
}

static void write_bit(BitWriter *writer, int bit)
{
    writer->buffer = (unsigned char)(writer->buffer * 2);
    if (bit != 0)
        writer->buffer++;
    writer->bits++;
    if (writer->bits == 8) {
        fputc(writer->buffer, writer->file);
        writer->buffer = 0;
        writer->bits = 0;
    }
}

static void write_code(BitWriter *writer, const unsigned char bits[], int length)
{
    int index;
    for (index = 0; index < length; index++)
        write_bit(writer, bits[index]);
}

static void bitwriter_flush(BitWriter *writer)
{
    if (writer->bits > 0) {
        writer->buffer <<= 8 - writer->bits;
        fputc(writer->buffer, writer->file);
        writer->buffer = 0;
        writer->bits = 0;
    }
}

static int write_uint64(FILE *file, uint64_t value)
{
    int byte;
    for (byte = 0; byte < 8; byte++) {
        if (fputc((int)(value & 0xff), file) == EOF)
            return 0;
        value >>= 8;
    }
    return 1;
}

static int count_frequencies(const char *filename, uint64_t frequencies[HUFFMAN_SYMBOLS],
                             uint64_t *total_bytes)
{
    FILE *file = fopen(filename, "rb");
    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;

    memset(frequencies, 0, sizeof(uint64_t) * HUFFMAN_SYMBOLS);
    *total_bytes = 0;
    if (file == NULL) {
        perror(filename);
        return 0;
    }
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        size_t index;
        for (index = 0; index < bytes_read; index++) {
            frequencies[buffer[index]]++;
            (*total_bytes)++;
        }
    }
    if (ferror(file)) {
        fprintf(stderr, "Error leyendo %s\n", filename);
        fclose(file);
        return 0;
    }
    fclose(file);
    return 1;
}

static int compress_file(const char *filename)
{
    uint64_t frequencies[HUFFMAN_SYMBOLS], original_size;
    unsigned char md5[MD5_DIGEST_LENGTH];
    char md5_hex[33], output_filename[PATH_MAX];
    HuffmanNode *root = NULL;
    HuffmanCode codes[HUFFMAN_SYMBOLS];
    FILE *input = NULL, *output = NULL;
    int symbol;

    if (!calculate_md5(filename, md5) ||
        !count_frequencies(filename, frequencies, &original_size))
        return 0;

    md5_to_hex(md5, md5_hex);
    printf("\nArchivo: %s\nMD5: %s\n", filename, md5_hex);
    snprintf(output_filename, sizeof(output_filename), "%s.huff", filename);
    output = fopen(output_filename, "wb");
    if (output == NULL) {
        perror(output_filename);
        return 0;
    }

    if (fwrite(MAGIC, 1, 4, output) != 4 || !write_uint64(output, original_size) ||
        fwrite(md5, 1, MD5_DIGEST_LENGTH, output) != MD5_DIGEST_LENGTH) {
        fclose(output);
        return 0;
    }
    for (symbol = 0; symbol < HUFFMAN_SYMBOLS; symbol++) {
        if (!write_uint64(output, frequencies[symbol])) {
            fclose(output);
            return 0;
        }
    }
    if (original_size == 0) {
        fclose(output);
        printf("Archivo vacío.\n");
        return 1;
    }

    root = huffman_build_tree(frequencies);
    if (root == NULL) {
        fclose(output);
        return 0;
    }
    huffman_generate_codes(root, codes);
    input = fopen(filename, "rb");
    if (input == NULL) {
        perror(filename);
        huffman_free_tree(root);
        fclose(output);
        return 0;
    }
    {
        BitWriter writer;
        unsigned char buffer[BUFFER_SIZE];
        size_t bytes_read;
        bitwriter_init(&writer, output);
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), input)) > 0) {
            size_t index;
            for (index = 0; index < bytes_read; index++)
                write_code(&writer, codes[buffer[index]].bits, codes[buffer[index]].length);
        }
        bitwriter_flush(&writer);
    }
    fclose(input);
    fclose(output);
    huffman_free_tree(root);
    printf("Comprimido: %s\n", output_filename);
    return 1;
}

int compress_directory(const char *directory)
{
    DIR *dir = opendir(directory);
    struct dirent *entry;
    int success = 1;

    if (dir == NULL) {
        perror(directory);
        return 0;
    }
    while ((entry = readdir(dir)) != NULL) {
        char path[PATH_MAX];
        struct stat status;
        size_t length;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
        if (stat(path, &status) != 0 || !S_ISREG(status.st_mode))
            continue;
        length = strlen(path);
        if (length >= 4 && strcmp(path + length - 4, ".txt") == 0 && !compress_file(path))
            success = 0;
    }
    closedir(dir);
    return success;
}