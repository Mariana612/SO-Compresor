#include "Codec.h"
#include "HuffmanTree.h"
#include "MD5Utils.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

/*
 * Formato del archivo .huff (un solo archivo por directorio):
 *
 *   | "HUF3" | cantidad N | tabla de N entradas | datos comprimidos de cada archivo |
 *   | 4 B    | 8 B        | N x ENTRY_SIZE      | resto del archivo                 |
 *
 * Cada entrada de la tabla guarda:
 *
 *   | nombre | tamaño original | tamaño comprimido | offset de datos | MD5  | frecuencias |
 *   | 256 B  | 8 B             | 8 B               | 8 B             | 16 B | 256 x 8 B   |
 *
 * El tamaño comprimido se calcula antes de codificar, así que la tabla se puede
 * escribir primero y cada trabajador (hilo o proceso) escribe o lee sus datos en
 * su propio offset, en paralelo y sin pisar a los demás.
 */

#define BLOCK_SIZE 65536   // Bytes que se leen del archivo
#define MAGIC "HUF3"       // Identificador
#define MAGIC_SIZE 4
#define ENTRY_SIZE (ARCHIVE_NAME_MAX + 3 * sizeof(uint64_t) + MD5_DIGEST_LENGTH + \
                    HUFFMAN_SYMBOLS * sizeof(uint64_t))
#define HEADER_SIZE(count) (MAGIC_SIZE + sizeof(uint64_t) + (count) * ENTRY_SIZE)

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

// Escribir entrada - Guarda los metadatos de un archivo en la tabla
static void write_entry(FILE *output, const ArchiveEntry *entry)
{
    fwrite(entry->name, 1, ARCHIVE_NAME_MAX, output);
    fwrite(&entry->original_size, sizeof(uint64_t), 1, output);
    fwrite(&entry->compressed_size, sizeof(uint64_t), 1, output);
    fwrite(&entry->data_offset, sizeof(uint64_t), 1, output);
    fwrite(entry->md5, 1, MD5_DIGEST_LENGTH, output);
    fwrite(entry->frequencies, sizeof(uint64_t), HUFFMAN_SYMBOLS, output);
}

// Leer entrada - Lee los metadatos guardados por write_entry
static int read_entry(FILE *input, ArchiveEntry *entry)
{
    fread(entry->name, 1, ARCHIVE_NAME_MAX, input);
    fread(&entry->original_size, sizeof(uint64_t), 1, input);
    fread(&entry->compressed_size, sizeof(uint64_t), 1, input);
    fread(&entry->data_offset, sizeof(uint64_t), 1, input);
    fread(entry->md5, 1, MD5_DIGEST_LENGTH, input);
    fread(entry->frequencies, sizeof(uint64_t), HUFFMAN_SYMBOLS, input);

    if (feof(input) || ferror(input)) // Incompleto
        return 0;

    entry->name[ARCHIVE_NAME_MAX - 1] = '\0';
    return 1;
}

// Nombre seguro - Evita que un .huff escriba fuera del directorio de salida
static int is_safe_name(const char *name)
{
    return name[0] != '\0' && strchr(name, '/') == NULL &&
           strcmp(name, ".") != 0 && strcmp(name, "..") != 0;
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
    return !ferror(output);
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
    return !ferror(output);
}


// FUNCIONES PRINCIPALES ---------------------------------------

// Analizar archivo - Calcula MD5, frecuencias y cuánto va a ocupar comprimido
int codec_analyze_file(const char *path, ArchiveEntry *entry)
{
    HuffmanCode codes[HUFFMAN_SYMBOLS];
    HuffmanNode *root;
    const char *name = strrchr(path, '/');
    uint64_t bits = 0;
    int symbol;

    // Se guarda solo el nombre, sin la carpeta
    name = (name == NULL) ? path : name + 1;
    if (strlen(name) >= ARCHIVE_NAME_MAX) {
        fprintf(stderr, "Error: nombre demasiado largo: %s\n", name);
        return 0;
    }
    memset(entry, 0, sizeof(*entry));
    strcpy(entry->name, name);

    // Firma MD5 y frecuencias
    if (!calculate_md5(path, entry->md5))
        return 0;
    if (!count_frequencies(path, entry->frequencies, &entry->original_size))
        return 0;
    if (entry->original_size == 0)
        return 1;

    // Tamaño comprimido = suma de (veces que aparece cada byte x largo de su código)
    root = huffman_build_tree(entry->frequencies);
    if (root == NULL)
        return 0;
    huffman_generate_codes(root, codes);
    clean_tree(root);

    for (symbol = 0; symbol < HUFFMAN_SYMBOLS; symbol++)
        bits += entry->frequencies[symbol] * (uint64_t)codes[symbol].length;
    entry->compressed_size = (bits + 7) / 8;
    return 1;
}

// Asignar offsets - Los datos de cada archivo van uno detrás del otro, después de la tabla
void codec_assign_offsets(ArchiveEntry entries[], size_t count)
{
    uint64_t offset = HEADER_SIZE(count);
    size_t i;

    for (i = 0; i < count; i++) {
        entries[i].data_offset = offset;
        offset += entries[i].compressed_size;
    }
}

// Tamaño total - Suma los tamaños originales de todos los archivos de la tabla
uint64_t codec_total_size(const ArchiveEntry entries[], size_t count)
{
    uint64_t total = 0;
    size_t i;

    for (i = 0; i < count; i++)
        total += entries[i].original_size;
    return total;
}

// Escribir cabecera - Crea el .huff con el identificador y la tabla de metadatos
int codec_write_header(const char *archive, const ArchiveEntry entries[], size_t count)
{
    uint64_t file_count = count;
    size_t i;
    int ok;
    FILE *output = fopen(archive, "wb");

    if (output == NULL) {
        fprintf(stderr, "Error: no se pudo crear %s\n", archive);
        return 0;
    }

    fwrite(MAGIC, 1, MAGIC_SIZE, output);                 // "HUF3"
    fwrite(&file_count, sizeof(uint64_t), 1, output);     // cantidad de archivos
    for (i = 0; i < count; i++)                           // tabla de metadatos
        write_entry(output, &entries[i]);

    ok = !ferror(output);
    if (fclose(output) != 0)
        ok = 0;
    return ok;
}

// Codificar archivo - Escribe los datos comprimidos en su lugar dentro del .huff
int codec_encode_file(const char *path, const char *archive, const ArchiveEntry *entry)
{
    HuffmanCode codes[HUFFMAN_SYMBOLS];
    HuffmanNode *root;
    FILE *output;
    off_t written = 0;
    int ok;

    if (entry->original_size > 0) {
        // Mismo árbol que en codec_analyze_file: sale de las mismas frecuencias
        root = huffman_build_tree(entry->frequencies);
        if (root == NULL)
            return 0;
        huffman_generate_codes(root, codes);
        clean_tree(root);

        // Cada trabajador abre el .huff por su cuenta y solo escribe en su región
        output = fopen(archive, "r+b");
        if (output == NULL) {
            fprintf(stderr, "Error: no se pudo abrir %s\n", archive);
            return 0;
        }

        ok = fseeko(output, (off_t)entry->data_offset, SEEK_SET) == 0 &&
             write_encoded_data(path, output, codes);
        if (ok)
            written = ftello(output) - (off_t)entry->data_offset;
        if (fclose(output) != 0)
            ok = 0;

        // Si el archivo cambió entre el análisis y la codificación, no calza
        if (ok && (uint64_t)written != entry->compressed_size)
            ok = 0;

        if (!ok) {
            fprintf(stderr, "Error comprimiendo %s\n", path);
            return 0;
        }
    }
    return 1;
}

// Leer cabecera - Lee y valida la tabla de metadatos del .huff
int codec_read_header(const char *archive, ArchiveEntry **entries, size_t *count)
{
    char magic[MAGIC_SIZE];
    uint64_t file_count, i;
    off_t archive_size;
    ArchiveEntry *table;
    FILE *input = fopen(archive, "rb");

    *entries = NULL;
    *count = 0;

    if (input == NULL) {
        fprintf(stderr, "Error: no se pudo abrir %s\n", archive);
        return 0;
    }

    // Tamaño total, para validar que la tabla y los datos quepan en el archivo
    fseeko(input, 0, SEEK_END);
    archive_size = ftello(input);
    rewind(input);

    fread(magic, 1, MAGIC_SIZE, input);
    fread(&file_count, sizeof(uint64_t), 1, input);
    if (feof(input) || ferror(input) || memcmp(magic, MAGIC, MAGIC_SIZE) != 0 ||
        file_count > ((uint64_t)archive_size - MAGIC_SIZE - sizeof(uint64_t)) / ENTRY_SIZE) {
        fprintf(stderr, "Error: %s no es un archivo .huff válido\n", archive);
        fclose(input);
        return 0;
    }

    table = malloc((file_count > 0 ? file_count : 1) * sizeof(ArchiveEntry));
    if (table == NULL) {
        fclose(input);
        return 0;
    }

    for (i = 0; i < file_count; i++) {
        if (!read_entry(input, &table[i]) ||
            table[i].data_offset > (uint64_t)archive_size ||
            table[i].compressed_size > (uint64_t)archive_size - table[i].data_offset) {
            fprintf(stderr, "Error: %s tiene una tabla de archivos inválida\n", archive);
            free(table);
            fclose(input);
            return 0;
        }
    }

    fclose(input);
    *entries = table;
    *count = (size_t)file_count;
    return 1;
}

// Descomprimir entrada - Expande un archivo del .huff y verifica su MD5
int codec_decode_entry(const char *archive, const ArchiveEntry *entry, const char *output_directory)
{
    HuffmanNode *root = NULL;
    char output_filename[PATH_MAX];
    FILE *input, *output;
    int ok;

    if (!is_safe_name(entry->name)) {
        fprintf(stderr, "Error: nombre inválido en %s: %s\n", archive, entry->name);
        return 0;
    }
    snprintf(output_filename, sizeof(output_filename), "%s/%s", output_directory, entry->name);

    // Reconstruir arbol
    if (entry->original_size > 0) {
        root = huffman_build_tree(entry->frequencies);
        if (root == NULL)
            return 0;
    }

    input = fopen(archive, "rb");
    if (input == NULL) {
        fprintf(stderr, "Error: no se pudo abrir %s\n", archive);
        clean_tree(root);
        return 0;
    }

    // Decodificar los datos desde su offset
    output = fopen(output_filename, "wb");
    if (output == NULL) {
        fprintf(stderr, "Error: no se pudo crear %s\n", output_filename);
        ok = 0;
    } else {
        ok = fseeko(input, (off_t)entry->data_offset, SEEK_SET) == 0 &&
             write_decoded_data(input, output, root, entry->original_size);
        if (fclose(output) != 0)
            ok = 0;

        // Comparar el MD5
        if (!ok)
            fprintf(stderr, "Error: datos incompletos para %s\n", entry->name);
        else if (!(ok = verify_md5(output_filename, entry->md5)))
            fprintf(stderr, "Error: la firma MD5 de %s no coincide\n", entry->name);

        // Fallo
        if (!ok)
            remove(output_filename);
    }

    //  Limpiar memoria y cerrar archivos
    fclose(input);
    clean_tree(root);
    return ok;
}
