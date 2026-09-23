#include "Codec.h"
#include "HuffmanTree.h"
#include "MD5Utils.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE (1024 * 1024)
#define MAGIC "HUF2"

typedef struct {
    FILE *file;
    unsigned char buffer;
    int bits;
} BitWriter;

typedef struct {
    FILE *file;
    unsigned char buffer;
    int bits;
} BitReader;

static void bitwriter_init(BitWriter *writer, FILE *file)
{
    writer->file = file;
    writer->buffer = 0;
    writer->bits = 0;
}

static void write_bit(BitWriter *writer, int bit)
{
    writer->buffer = (unsigned char)(writer->buffer * 2 + (bit != 0));
    if (++writer->bits == 8) {
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
    if (writer->bits > 0)
        fputc((unsigned char)(writer->buffer << (8 - writer->bits)), writer->file);
}

static int write_uint64(FILE *file, uint64_t value)
{
    int byte;
    for (byte = 0; byte < 8; byte++, value >>= 8)
        if (fputc((int)(value & 0xff), file) == EOF)
            return 0;
    return 1;
}

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

static int count_frequencies(const char *filename, uint64_t frequencies[HUFFMAN_SYMBOLS],
                            uint64_t *total_bytes)
{
    FILE *file = fopen(filename, "rb");
    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;
    memset(frequencies, 0, sizeof(uint64_t) * HUFFMAN_SYMBOLS);
    *total_bytes = 0;
    if (file == NULL)
        return 0;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        size_t index;
        for (index = 0; index < bytes_read; index++) {
            frequencies[buffer[index]]++;
            (*total_bytes)++;
        }
    }
    if (ferror(file)) {
        fclose(file);
        return 0;
    }
    fclose(file);
    return 1;
}

int common_compress_file(const char *filename)
{
    uint64_t frequencies[HUFFMAN_SYMBOLS], original_size;
    unsigned char md5[MD5_DIGEST_LENGTH];
    char output_filename[PATH_MAX];
    HuffmanNode *root = NULL;
    HuffmanCode codes[HUFFMAN_SYMBOLS];
    FILE *input = NULL, *output = NULL;
    int symbol, ok = 0;

    if (!calculate_md5(filename, md5) ||
        !count_frequencies(filename, frequencies, &original_size))
        return 0;
    snprintf(output_filename, sizeof(output_filename), "%s.huff", filename);
    output = fopen(output_filename, "wb");
    if (output == NULL)
        return 0;
    if (fwrite(MAGIC, 1, 4, output) != 4 || !write_uint64(output, original_size) ||
        fwrite(md5, 1, MD5_DIGEST_LENGTH, output) != MD5_DIGEST_LENGTH)
        goto done;
    for (symbol = 0; symbol < HUFFMAN_SYMBOLS; symbol++)
        if (!write_uint64(output, frequencies[symbol]))
            goto done;
    if (original_size == 0) {
        ok = 1;
        goto done;
    }
    root = huffman_build_tree(frequencies);
    if (root == NULL)
        goto done;
    huffman_generate_codes(root, codes);
    input = fopen(filename, "rb");
    if (input == NULL)
        goto done;
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
        if (ferror(input))
            goto done;
        bitwriter_flush(&writer);
    }
    ok = 1;
done:
    if (input != NULL)
        fclose(input);
    if (output != NULL)
        fclose(output);
    if (root != NULL)
        huffman_free_tree(root);
    if (!ok)
        remove(output_filename);
    if (ok)
        printf("Comprimido: %s\n", output_filename);
    return ok;
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

int common_decompress_file(const char *compressed_filename, const char *output_directory)
{
    FILE *input = fopen(compressed_filename, "rb");
    FILE *output = NULL;
    char output_filename[PATH_MAX], magic[5];
    uint64_t frequencies[HUFFMAN_SYMBOLS], original_size, bytes_written = 0;
    unsigned char expected_md5[MD5_DIGEST_LENGTH];
    HuffmanNode *root = NULL, *current;
    int symbol, ok = 0;

    if (input == NULL)
        return 0;
    if (fread(magic, 1, 4, input) != 4 || memcmp(magic, MAGIC, 4) != 0 ||
        !read_uint64(input, &original_size) ||
        fread(expected_md5, 1, MD5_DIGEST_LENGTH, input) != MD5_DIGEST_LENGTH)
        goto done;
    for (symbol = 0; symbol < HUFFMAN_SYMBOLS; symbol++)
        if (!read_uint64(input, &frequencies[symbol]))
            goto done;
    if (!output_name(compressed_filename, output_directory, output_filename))
        goto done;
    output = fopen(output_filename, "wb");
    if (output == NULL)
        goto done;
    if (original_size > 0) {
        root = huffman_build_tree(frequencies);
        if (root == NULL)
            goto done;
        current = root;
        {
            BitReader reader;
            bitreader_init(&reader, input);
            while (bytes_written < original_size) {
                int bit;
                if (huffman_is_leaf(root)) {
                    if (fputc(root->symbol, output) == EOF)
                        goto done;
                    bytes_written++;
                    continue;
                }
                bit = read_bit(&reader);
                if (bit < 0)
                    goto done;
                current = bit == 0 ? current->left : current->right;
                if (current == NULL)
                    goto done;
                if (huffman_is_leaf(current)) {
                    if (fputc(current->symbol, output) == EOF)
                        goto done;
                    bytes_written++;
                    current = root;
                }
            }
        }
    }
    {
        int close_result = fclose(output);
        output = NULL;
        if (close_result != 0)
            goto done;
    }
    if (bytes_written != original_size || !verify_md5(output_filename, expected_md5))
        goto done;
    output = NULL;
    ok = 1;
done:
    if (root != NULL)
        huffman_free_tree(root);
    if (output != NULL)
        fclose(output);
    fclose(input);
    if (!ok && output_name(compressed_filename, output_directory, output_filename))
        remove(output_filename);
    if (ok)
        printf("Archivo descomprimido: %s\n", output_filename);
    return ok;
}
