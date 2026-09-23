#include "Decompressor.h"

#include "HuffmanTree.h"
#include "MD5Utils.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAGIC "HUF2"

/* ============================================================
   LEER UN ARCHIVO
   ============================================================ */


// ESTRUCTURAS DE DATOS ---------------------------------------
typedef struct {
    FILE *file;
    unsigned char buffer;
    int bits;
} BitReader;

// FUNCIONES AUXILIARES ---------------------------------------
static int read_uint64(FILE *file, uint64_t *value)
{
    uint64_t result = 0;
    int byte;
    for (byte = 0; byte < 8; byte++) {
        int value_read = fgetc(file);
        if (value_read == EOF)
            return 0;
        result |= (uint64_t)(unsigned char)value_read << (8 * byte);
    }
    *value = result;
    return 1;
}

static void bitreader_init(BitReader *reader, FILE *file)
{
    reader->file = file;
    reader->buffer = 0;
    reader->bits = 0;
}

static int read_bit(BitReader *reader)
{
    if (reader->bits == 0) {
        int value = fgetc(reader->file);
        if (value == EOF)
            return -1;
        reader->buffer = (unsigned char)value;
        reader->bits = 8;
    }
    {
        int bit = (reader->buffer >> 7) & 1;
        reader->buffer <<= 1;
        reader->bits--;
        return bit;
    }
}

static int output_name(const char *compressed_filename, const char *output_directory,
                       char output_filename[PATH_MAX])
{
    const char *base = strrchr(compressed_filename, '/');
    size_t length;

    base = base == NULL ? compressed_filename : base + 1;
    length = strlen(base);
    if (length <= 5 || strcmp(base + length - 5, ".huff") != 0)
        return 0;
    snprintf(output_filename, PATH_MAX, "%s/%.*s", output_directory,
             (int)(length - 5), base);
    return 1;
}


/* ============================================================
   DESCOMPRIMIR
   ============================================================ */

   // FUNCIONES PRINCIPALES ---------------------------------------
int decompress_file(const char *compressed_filename, const char *output_directory)
{
    FILE *input = fopen(compressed_filename, "rb");
    FILE *output = NULL;
    char output_filename[PATH_MAX], magic[5];
    uint64_t frequencies[HUFFMAN_SYMBOLS], original_size, bytes_written = 0;
    unsigned char expected_md5[MD5_DIGEST_LENGTH];
    HuffmanNode *root = NULL, *current;
    int symbol;

    if (input == NULL) {
        perror(compressed_filename);
        return 0;
    }
    if (fread(magic, 1, 4, input) != 4 || memcmp(magic, MAGIC, 4) != 0 ||
        !read_uint64(input, &original_size) ||
        fread(expected_md5, 1, MD5_DIGEST_LENGTH, input) != MD5_DIGEST_LENGTH) {
        fprintf(stderr, "Error: archivo comprimido incompleto o formato inválido.\n");
        fclose(input);
        return 0;
    }
    for (symbol = 0; symbol < HUFFMAN_SYMBOLS; symbol++) {
        if (!read_uint64(input, &frequencies[symbol])) {
            fprintf(stderr, "Error leyendo tabla de frecuencias.\n");
            fclose(input);
            return 0;
        }
    }
    if (!output_name(compressed_filename, output_directory, output_filename)) {
        fprintf(stderr, "Error: el archivo debe terminar en .huff.\n");
        fclose(input);
        return 0;
    }
    output = fopen(output_filename, "wb");
    if (output == NULL) {
        perror(output_filename);
        fclose(input);
        return 0;
    }
    if (original_size > 0) {
        root = huffman_build_tree(frequencies);
        if (root == NULL) {
            fclose(output);
            fclose(input);
            return 0;
        }
        current = root;
        {
            BitReader reader;
            bitreader_init(&reader, input);
            while (bytes_written < original_size) {
                int bit;
                if (huffman_is_leaf(root)) {
                    if (fputc(root->symbol, output) == EOF)
                        break;
                    bytes_written++;
                    continue;
                }
                bit = read_bit(&reader);
                if (bit < 0) {
                    fprintf(stderr, "Error: datos comprimidos incompletos.\n");
                    break;
                }
                current = bit == 0 ? current->left : current->right;
                if (current == NULL)
                    break;
                if (huffman_is_leaf(current)) {
                    if (fputc(current->symbol, output) == EOF)
                        break;
                    bytes_written++;
                    current = root;
                }
            }
        }
        huffman_free_tree(root);
        root = NULL;
    }
    fclose(output);
    fclose(input);
    if (bytes_written != original_size || !verify_md5(output_filename, expected_md5)) {
        remove(output_filename);
        return 0;
    }
    printf("Archivo descomprimido: %s\n", output_filename);
    return 1;
}