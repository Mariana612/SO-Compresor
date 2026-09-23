#include "Codec.h"
#include "HuffmanTree.h"
#include "MD5Utils.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Formato del archivo .huff:
 *
 *   | "HUF2" | tamaño original | firma MD5 | tabla de frecuencias | datos comprimidos |
 *   | 4 B    | 8 B             | 16 B      | 256 x 8 B            | resto del archivo |
 *
 * Con la tabla de frecuencias el descompresor puede reconstruir exactamente el
 * mismo árbol de Huffman que usó el compresor.
 */

#define BLOCK_SIZE 65536   // Bytes que se leen del archivo 
#define MAGIC "HUF2"       // Identificador 
#define MAGIC_SIZE 4

// ============================================================
// ESCRITURA DE BITS 
// ============================================================


// ESTRUCTURAS DE DATOS ---------------------------------------

typedef struct {
    FILE *file;
    unsigned char current_byte;  // Byte que se está armando
    int bit_count;               // Cuántos bits tiene 
} BitWriter;


// FUNCIONES PRINCIPALES ----------------------------------------

// Iniciar escritor - inicializador
static void bitwriter_init(BitWriter *writer, FILE *file)
{
    writer->file = file;
    writer->current_byte = 0;
    writer->bit_count = 0;
}

// Escribir bit - Agrega un bit al byte actual y lo escribe si ya está lleno
static void write_bit(BitWriter *writer, int bit)
{
    writer->current_byte = (writer->current_byte << 1) | (bit & 1);
    writer->bit_count++;

    if (writer->bit_count == 8) {
        fputc(writer->current_byte, writer->file);
        writer->current_byte = 0;
        writer->bit_count = 0;
    }
}

// Terminar escritura - Escribe el último byte incompleto rellenando con ceros
static void bitwriter_finish(BitWriter *writer)
{
    if (writer->bit_count > 0) {
        writer->current_byte <<= (8 - writer->bit_count);
        fputc(writer->current_byte, writer->file);
    }
}


// ============================================================
// LECTURA DE BITS 
// ============================================================

// ESTRUCTURAS DE DATOS ---------------------------------------

typedef struct {
    FILE *file;
    unsigned char current_byte;  // Último byte leído
    int bits_left;               // Bits que faltan 
} BitReader;

// FUNCIONES PRINCIPALES ----------------------------------------

// Iniciar lector - Inicializacion
static void bitreader_init(BitReader *reader, FILE *file)
{
    reader->file = file;
    reader->current_byte = 0;
    reader->bits_left = 0;
}

// Leer bit - Lee el siguiente bit del archivo
static int read_bit(BitReader *reader)
{
    int bit;

    if (reader->bits_left == 0) {
        int next_byte = fgetc(reader->file);
        if (next_byte == EOF) // End of File
            return -1;
        reader->current_byte = (unsigned char)next_byte;
        reader->bits_left = 8;
    }

    // Se entrega el bit más significativo y se corre el byte a la izquierda
    bit = (reader->current_byte >> 7) & 1;
    reader->current_byte <<= 1;
    reader->bits_left--;
    return bit;
}


// ============================================================
// PREPARARACION DE ARCHIVOS
// ============================================================

// FUNCIONES AUXILIARES ----------------------------------------

// Contar frecuencias - Cuenta cuántas veces aparece cada byte en el archivo
static int count_frequencies(const char *filename, uint64_t frequencies[HUFFMAN_SYMBOLS],
                             uint64_t *file_size)
{
    unsigned char block[BLOCK_SIZE];
    size_t bytes_read, i;
    FILE *file = fopen(filename, "rb");

    if (file == NULL)
        return 0;

    memset(frequencies, 0, sizeof(uint64_t) * HUFFMAN_SYMBOLS);
    *file_size = 0;

    while ((bytes_read = fread(block, 1, BLOCK_SIZE, file)) > 0) {
        for (i = 0; i < bytes_read; i++)
            frequencies[block[i]]++;
        *file_size += bytes_read;
    }

    fclose(file);
    return 1;
}

// Escribir cabecera - Guarda el identificador, tamaño, MD5 y frecuencias
static int write_header(FILE *output, uint64_t original_size,
                        const unsigned char md5[MD5_DIGEST_LENGTH],
                        const uint64_t frequencies[HUFFMAN_SYMBOLS])
{
    fwrite(MAGIC, 1, MAGIC_SIZE, output);                          // "HUF2"
    fwrite(&original_size, sizeof(uint64_t), 1, output);           // tamaño original
    fwrite(md5, 1, MD5_DIGEST_LENGTH, output);                     // firma MD5
    fwrite(frequencies, sizeof(uint64_t), HUFFMAN_SYMBOLS, output); // tabla de frecuencias

    
    return !ferror(output); // error check
}

// Leer cabecera - Lee y valida los datos guardados por write_header
static int read_header(FILE *input, uint64_t *original_size,
                       unsigned char md5[MD5_DIGEST_LENGTH],
                       uint64_t frequencies[HUFFMAN_SYMBOLS])
{
    char magic[MAGIC_SIZE];

    // Se lee 
    fread(magic, 1, MAGIC_SIZE, input);
    fread(original_size, sizeof(uint64_t), 1, input);
    fread(md5, 1, MD5_DIGEST_LENGTH, input);
    fread(frequencies, sizeof(uint64_t), HUFFMAN_SYMBOLS, input);

    if (feof(input) || ferror(input)) // Incompleto
        return 0;

    if (memcmp(magic, MAGIC, MAGIC_SIZE) != 0) // Formato inválido
        return 0;

    return 1;
}

// Codificar datos - Reemplaza cada byte del original por su código de Huffman
static int write_encoded_data(const char *filename, FILE *output,
                              const HuffmanCode codes[HUFFMAN_SYMBOLS])
{
    unsigned char block[BLOCK_SIZE];
    size_t bytes_read, i;
    int bit;
    BitWriter writer;
    FILE *input = fopen(filename, "rb");

    if (input == NULL)
        return 0;

    bitwriter_init(&writer, output);
    while ((bytes_read = fread(block, 1, BLOCK_SIZE, input)) > 0) {
        for (i = 0; i < bytes_read; i++) {
            const HuffmanCode *code = &codes[block[i]];
            for (bit = 0; bit < code->length; bit++)
                write_bit(&writer, code->bits[bit]);
        }
    }
    bitwriter_finish(&writer);

    fclose(input);
    return 1;
}

// Decodificar datos - Recorre el árbol bit por bit hasta recuperar cada byte
static int write_decoded_data(FILE *input, FILE *output, const HuffmanNode *root,
                              uint64_t original_size)
{
    BitReader reader;
    const HuffmanNode *current = root;
    uint64_t bytes_written = 0;
    int bit;

    bitreader_init(&reader, input);
    while (bytes_written < original_size) {

        if (is_leaf(root)) { // check especial
            fputc(root->symbol, output);
            bytes_written++;
            continue;
        }

        bit = read_bit(&reader);
        if (bit < 0)
            return 0;  // incompleto

        
        if (bit == 0) // izquierda
            current = current->left;
        else // derecha
            current = current->right;

        // raiz
        if (is_leaf(current)) {
            fputc(current->symbol, output);
            bytes_written++;
            current = root;
        }
    }
    return 1;
}

// Nombre de salida 
static int build_output_name(const char *compressed_filename, const char *output_directory,
                             char output_filename[PATH_MAX])
{
    const char *name = strrchr(compressed_filename, '/');
    size_t length;

    // Se quita la carpeta y se deja solo el nombre del archivo
    if (name == NULL)
        name = compressed_filename;
    else
        name = name + 1;

    // FORMATO DE NOMBRE
    length = strlen(name);
    if (length <= 5 || strcmp(name + length - 5, ".huff") != 0)
        return 0;

   
    snprintf(output_filename, PATH_MAX, "%s/%.*s", output_directory, (int)(length - 5), name);
    return 1;
}


// FUNCIONES PRINCIPALES ---------------------------------------

// Comprimir archivo 
int common_compress_file(const char *filename)
{
    uint64_t frequencies[HUFFMAN_SYMBOLS];
    uint64_t original_size;
    unsigned char md5[MD5_DIGEST_LENGTH];
    HuffmanCode codes[HUFFMAN_SYMBOLS];
    HuffmanNode *root = NULL;
    char output_filename[PATH_MAX];
    FILE *output;
    int ok;

    // Firma MD5 y frecuencias 
    if (!calculate_md5(filename, md5))
        return 0;
    if (!count_frequencies(filename, frequencies, &original_size))
        return 0;

    // Árbol de Huffman y tabla de códigos 
    if (original_size > 0) {
        root = huffman_build_tree(frequencies);
        if (root == NULL)
            return 0;
        huffman_generate_codes(root, codes);
    }

    // Escribir el archivo comprimido
    snprintf(output_filename, sizeof(output_filename), "%s.huff", filename);
    output = fopen(output_filename, "wb");
    if (output == NULL) {
        fprintf(stderr, "Error: no se pudo crear %s\n", output_filename);
        clean_tree(root);
        return 0;
    }

    ok = write_header(output, original_size, md5, frequencies);
    if (ok && original_size > 0)
        ok = write_encoded_data(filename, output, codes);
    fclose(output);
    clean_tree(root);

    // Si algo falló 
    if (!ok) {
        fprintf(stderr, "Error comprimiendo %s\n", filename);
        remove(output_filename);
        return 0;
    }
    printf("Comprimido: %s\n", output_filename);
    return 1;
}

// Descomprimir archivo -
int common_decompress_file(const char *compressed_filename, const char *output_directory)
{
    uint64_t frequencies[HUFFMAN_SYMBOLS];
    uint64_t original_size;
    unsigned char saved_md5[MD5_DIGEST_LENGTH];
    HuffmanNode *root = NULL;
    char output_filename[PATH_MAX];
    FILE *input, *output;
    int ok;

    input = fopen(compressed_filename, "rb");
    if (input == NULL) {
        fprintf(stderr, "Error: no se pudo abrir %s\n", compressed_filename);
        return 0;
    }

    // Leer la cabecera del archivo c
    ok = read_header(input, &original_size, saved_md5, frequencies) &&
         build_output_name(compressed_filename, output_directory, output_filename);

    
    if (!ok)
        fprintf(stderr, "Error: %s no es un archivo .huff válido\n", compressed_filename);

    // Reconstruir arbol
    if (ok && original_size > 0) {
        root = huffman_build_tree(frequencies);
        if (root == NULL)
            ok = 0;
    }

    // Decodificar los datos
    if (ok) {
        output = fopen(output_filename, "wb");
        if (output == NULL) {
            fprintf(stderr, "Error: no se pudo crear %s\n", output_filename);
            ok = 0;
        } else {
            ok = write_decoded_data(input, output, root, original_size);
            fclose(output);

            // Comparar el MD5 
            if (ok)
                ok = verify_md5(output_filename, saved_md5);

            // Fallo
            if (!ok)
                remove(output_filename);
        }
    }

    //  Limpiar memoria y cerrar archivos
    fclose(input);
    clean_tree(root);

    if (ok)
        printf("Archivo descomprimido: %s\n", output_filename);
    return ok;
}
