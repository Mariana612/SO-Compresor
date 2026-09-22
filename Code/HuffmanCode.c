/*
 * huffman.c
 *
 * Compresor Huffman para archivos TXT grandes.
 *
 * Características:
 *
 *  - Procesamiento por bloques.
 *  - Compatible con UTF-8 y caracteres especiales.
 *  - Huffman sobre los 256 posibles valores de byte.
 *  - MD5 del archivo original.
 *  - MD5 almacenado dentro del archivo .huff.
 *
 * Compilación:
 *
 *     gcc -Wall -Wextra -O2 huffman.c -o huffman -lcrypto
 *
 * Uso:
 *
 *     ./huffman libros/
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>

#include <openssl/md5.h>

#define SYMBOLS 256
#define MAX_CODE_LENGTH 256

#define BUFFER_SIZE (1024 * 1024)

/*
 * Nuevo formato.
 */
#define MAGIC "HUF2"


/* ============================================================
   NODO DE HUFFMAN
   ============================================================ */

typedef struct HuffmanNode {

    unsigned char symbol;

    uint64_t frequency;

    struct HuffmanNode *left;

    struct HuffmanNode *right;

} HuffmanNode;


/* ============================================================
   MIN HEAP
   ============================================================ */

typedef struct {

    HuffmanNode **nodes;

    int size;

    int capacity;

} MinHeap;


/* ============================================================
   CÓDIGO HUFFMAN
   ============================================================ */

typedef struct {

    unsigned char bits[MAX_CODE_LENGTH];

    int length;

} HuffmanCode;


/* ============================================================
   MEMORIA
   ============================================================ */

static void *safe_malloc(size_t size)
{
    void *ptr = malloc(size);

    if (ptr == NULL) {

        fprintf(stderr,
                "Error: memoria insuficiente.\n");

        exit(EXIT_FAILURE);
    }

    return ptr;
}


/* ============================================================
   CREAR NODO
   ============================================================ */

static HuffmanNode *create_node(
        unsigned char symbol,
        uint64_t frequency,
        HuffmanNode *left,
        HuffmanNode *right)
{
    HuffmanNode *node;

    node = safe_malloc(sizeof(HuffmanNode));

    node->symbol = symbol;

    node->frequency = frequency;

    node->left = left;

    node->right = right;

    return node;
}


/* ============================================================
   HOJA
   ============================================================ */

static int is_leaf(HuffmanNode *node)
{
    return node->left == NULL &&
           node->right == NULL;
}


/* ============================================================
   LIBERAR ÁRBOL
   ============================================================ */

static void free_tree(HuffmanNode *root)
{
    if (root == NULL)
        return;

    free_tree(root->left);

    free_tree(root->right);

    free(root);
}


/* ============================================================
   HEAP
   ============================================================ */

static MinHeap *create_heap(int capacity)
{
    MinHeap *heap;

    heap = safe_malloc(sizeof(MinHeap));

    heap->nodes =
        safe_malloc(
            sizeof(HuffmanNode *) * capacity
        );

    heap->size = 0;

    heap->capacity = capacity;

    return heap;
}


static void swap_nodes(
        HuffmanNode **a,
        HuffmanNode **b)
{
    HuffmanNode *tmp;

    tmp = *a;

    *a = *b;

    *b = tmp;
}


static void heap_push(
        MinHeap *heap,
        HuffmanNode *node)
{
    int i;

    heap->nodes[heap->size] = node;

    i = heap->size;

    heap->size++;

    while (i > 0) {

        int parent =
            (i - 1) / 2;

        if (heap->nodes[parent]->frequency <=
            heap->nodes[i]->frequency) {

            break;
        }

        swap_nodes(
            &heap->nodes[parent],
            &heap->nodes[i]
        );

        i = parent;
    }
}


static HuffmanNode *heap_pop(
        MinHeap *heap)
{
    HuffmanNode *result;

    int i;

    if (heap->size == 0)
        return NULL;

    result = heap->nodes[0];

    heap->size--;

    if (heap->size == 0)
        return result;

    heap->nodes[0] =
        heap->nodes[heap->size];

    i = 0;

    while (1) {

        int left =
            2 * i + 1;

        int right =
            2 * i + 2;

        int smallest = i;

        if (left < heap->size &&
            heap->nodes[left]->frequency <
            heap->nodes[smallest]->frequency) {

            smallest = left;
        }

        if (right < heap->size &&
            heap->nodes[right]->frequency <
            heap->nodes[smallest]->frequency) {

            smallest = right;
        }

        if (smallest == i)
            break;

        swap_nodes(
            &heap->nodes[i],
            &heap->nodes[smallest]
        );

        i = smallest;
    }

    return result;
}


static void destroy_heap(MinHeap *heap)
{
    if (heap == NULL)
        return;

    free(heap->nodes);

    free(heap);
}


/* ============================================================
   CONSTRUIR ÁRBOL
   ============================================================ */

static HuffmanNode *build_tree(
        uint64_t frequencies[SYMBOLS])
{
    MinHeap *heap;

    int i;

    heap =
        create_heap(SYMBOLS);


    /*
     * Crear hojas.
     */

    for (i = 0; i < SYMBOLS; i++) {

        if (frequencies[i] > 0) {

            HuffmanNode *node;

            node =
                create_node(
                    (unsigned char)i,
                    frequencies[i],
                    NULL,
                    NULL
                );

            heap_push(
                heap,
                node
            );
        }
    }


    if (heap->size == 0) {

        destroy_heap(heap);

        return NULL;
    }


    /*
     * Caso especial:
     * solamente existe un byte.
     */

    if (heap->size == 1) {

        HuffmanNode *only;

        only = heap_pop(heap);

        HuffmanNode *root =
            create_node(
                0,
                only->frequency,
                only,
                NULL
            );

        destroy_heap(heap);

        return root;
    }


    /*
     * Construcción del árbol.
     */

    while (heap->size > 1) {

        HuffmanNode *left =
            heap_pop(heap);

        HuffmanNode *right =
            heap_pop(heap);

        HuffmanNode *parent =
            create_node(
                0,
                left->frequency +
                right->frequency,
                left,
                right
            );

        heap_push(
            heap,
            parent
        );
    }


    HuffmanNode *root =
        heap_pop(heap);

    destroy_heap(heap);

    return root;
}


/* ============================================================
   GENERAR CÓDIGOS
   ============================================================ */

static void generate_codes_recursive(
        HuffmanNode *node,
        unsigned char path[],
        int depth,
        HuffmanCode codes[SYMBOLS])
{
    if (node == NULL)
        return;


    if (is_leaf(node)) {

        /*
         * Si existe solamente un símbolo,
         * asignamos el código 0.
         */

        if (depth == 0) {

            path[0] = 0;

            depth = 1;
        }


        memcpy(
            codes[node->symbol].bits,
            path,
            depth
        );

        codes[node->symbol].length =
            depth;

        return;
    }


    /*
     * Izquierda = 0
     */

    path[depth] = 0;

    generate_codes_recursive(
        node->left,
        path,
        depth + 1,
        codes
    );


    /*
     * Derecha = 1
     */

    path[depth] = 1;

    generate_codes_recursive(
        node->right,
        path,
        depth + 1,
        codes
    );
}


static void generate_codes(
        HuffmanNode *root,
        HuffmanCode codes[SYMBOLS])
{
    unsigned char path[MAX_CODE_LENGTH];

    memset(
        codes,
        0,
        sizeof(HuffmanCode) * SYMBOLS
    );

    generate_codes_recursive(
        root,
        path,
        0,
        codes
    );
}


/* ============================================================
   BIT WRITER
   ============================================================ */

typedef struct {

    FILE *file;

    unsigned char buffer;

    int bits;

} BitWriter;


static void bitwriter_init(
        BitWriter *writer,
        FILE *file)
{
    writer->file = file;

    writer->buffer = 0;

    writer->bits = 0;
}


static void write_bit(
        BitWriter *writer,
        int bit)
{
    writer->buffer <<= 1;

    if (bit)
        writer->buffer |= 1;

    writer->bits++;


    if (writer->bits == 8) {

        fputc(
            writer->buffer,
            writer->file
        );

        writer->buffer = 0;

        writer->bits = 0;
    }
}


static void write_code(
        BitWriter *writer,
        HuffmanCode *code)
{
    int i;

    for (i = 0;
         i < code->length;
         i++) {

        write_bit(
            writer,
            code->bits[i]
        );
    }
}


static void bitwriter_flush(
        BitWriter *writer)
{
    if (writer->bits > 0) {

        writer->buffer <<=
            8 - writer->bits;

        fputc(
            writer->buffer,
            writer->file
        );

        writer->buffer = 0;

        writer->bits = 0;
    }
}


/* ============================================================
   UINT64
   ============================================================ */

static int write_uint64(
        FILE *file,
        uint64_t value)
{
    int i;

    for (i = 0; i < 8; i++) {

        if (fputc(
                (int)(value & 0xFF),
                file
            ) == EOF) {

            return 0;
        }

        value >>= 8;
    }

    return 1;
}


/* ============================================================
   MD5
   ============================================================ */

/*
 * Calcula el MD5 leyendo el archivo por bloques.
 *
 * md5_out debe tener 16 bytes.
 */

static int calculate_md5(
        const char *filename,
        unsigned char md5_out[MD5_DIGEST_LENGTH])
{
    FILE *file;

    unsigned char buffer[BUFFER_SIZE];

    size_t bytes_read;

    MD5_CTX md5;


    file = fopen(
        filename,
        "rb"
    );

    if (file == NULL) {

        perror(filename);

        return 0;
    }


    /*
     * Inicializar MD5.
     */

    MD5_Init(&md5);


    /*
     * Procesar el archivo por bloques.
     */

    while ((bytes_read =
            fread(
                buffer,
                1,
                BUFFER_SIZE,
                file
            )) > 0) {

        MD5_Update(
            &md5,
            buffer,
            bytes_read
        );
    }


    if (ferror(file)) {

        fprintf(
            stderr,
            "Error leyendo %s\n",
            filename
        );

        fclose(file);

        return 0;
    }


    /*
     * Obtener resultado final.
     */

    MD5_Final(
        md5_out,
        &md5
    );


    fclose(file);

    return 1;
}


/* ============================================================
   MD5 COMO TEXTO HEXADECIMAL
   ============================================================ */

static void md5_to_hex(
        const unsigned char md5[16],
        char hex[33])
{
    static const char digits[] =
        "0123456789abcdef";

    int i;

    for (i = 0; i < 16; i++) {

        hex[i * 2] =
            digits[
                (md5[i] >> 4) & 0x0F
            ];

        hex[i * 2 + 1] =
            digits[
                md5[i] & 0x0F
            ];
    }

    hex[32] = '\0';
}


/* ============================================================
   CONTAR FRECUENCIAS
   ============================================================ */

static int count_frequencies(
        const char *filename,
        uint64_t frequencies[SYMBOLS],
        uint64_t *total_bytes)
{
    FILE *file;

    unsigned char buffer[BUFFER_SIZE];

    size_t bytes_read;

    memset(
        frequencies,
        0,
        sizeof(uint64_t) * SYMBOLS
    );

    *total_bytes = 0;


    file = fopen(
        filename,
        "rb"
    );

    if (file == NULL) {

        perror(filename);

        return 0;
    }


    while ((bytes_read =
            fread(
                buffer,
                1,
                BUFFER_SIZE,
                file
            )) > 0) {

        size_t i;

        for (i = 0;
             i < bytes_read;
             i++) {

            frequencies[
                buffer[i]
            ]++;

            (*total_bytes)++;
        }
    }


    if (ferror(file)) {

        fprintf(
            stderr,
            "Error leyendo %s\n",
            filename
        );

        fclose(file);

        return 0;
    }


    fclose(file);

    return 1;
}


/* ============================================================
   COMPRIMIR
   ============================================================ */

static int compress_file(
        const char *filename)
{
    uint64_t frequencies[SYMBOLS];

    uint64_t original_size;

    unsigned char md5[MD5_DIGEST_LENGTH];

    char md5_hex[33];

    HuffmanNode *root;

    HuffmanCode codes[SYMBOLS];

    FILE *input;

    FILE *output;

    char output_filename[PATH_MAX];

    unsigned char buffer[BUFFER_SIZE];

    size_t bytes_read;

    int i;


    /*
     * ========================================================
     * PASO 1
     * Calcular MD5
     * ========================================================
     */

    if (!calculate_md5(
            filename,
            md5)) {

        return 0;
    }


    md5_to_hex(
        md5,
        md5_hex
    );


    printf(
        "\nArchivo: %s\n",
        filename
    );

    printf(
        "MD5: %s\n",
        md5_hex
    );


    /*
     * ========================================================
     * PASO 2
     * Contar frecuencias
     * ========================================================
     */

    if (!count_frequencies(
            filename,
            frequencies,
            &original_size)) {

        return 0;
    }


    /*
     * Nombre del archivo comprimido.
     */

    snprintf(
        output_filename,
        sizeof(output_filename),
        "%s.huff",
        filename
    );


    output =
        fopen(
            output_filename,
            "wb"
        );

    if (output == NULL) {

        perror(output_filename);

        return 0;
    }


    /*
     * ========================================================
     * ENCABEZADO
     * ========================================================
     *
     * 4 bytes:
     *     HUF2
     *
     * 8 bytes:
     *     tamaño original
     *
     * 16 bytes:
     *     MD5
     *
     * 2048 bytes:
     *     frecuencias
     *
     * 256 * 8 = 2048
     */

    fwrite(
        MAGIC,
        1,
        4,
        output
    );


    /*
     * Tamaño original.
     */

    if (!write_uint64(
            output,
            original_size)) {

        fclose(output);

        return 0;
    }


    /*
     * MD5.
     *
     * Se almacena directamente como
     * 16 bytes binarios.
     */

    if (fwrite(
            md5,
            1,
            MD5_DIGEST_LENGTH,
            output
        ) != MD5_DIGEST_LENGTH) {

        fprintf(
            stderr,
            "Error escribiendo MD5.\n"
        );

        fclose(output);

        return 0;
    }


    /*
     * Tabla de frecuencias.
     */

    for (i = 0;
         i < SYMBOLS;
         i++) {

        if (!write_uint64(
                output,
                frequencies[i])) {

            fclose(output);

            return 0;
        }
    }


    /*
     * Archivo vacío.
     */

    if (original_size == 0) {

        fclose(output);

        printf(
            "Archivo vacío.\n"
        );

        return 1;
    }


    /*
     * ========================================================
     * PASO 3
     * Construir árbol
     * ========================================================
     */

    root =
        build_tree(
            frequencies
        );

    if (root == NULL) {

        fclose(output);

        return 0;
    }


    /*
     * Generar códigos.
     */

    generate_codes(
        root,
        codes
    );


    /*
     * ========================================================
     * PASO 4
     * Codificar
     * ========================================================
     */

    input =
        fopen(
            filename,
            "rb"
        );

    if (input == NULL) {

        perror(filename);

        free_tree(root);

        fclose(output);

        return 0;
    }


    {
        BitWriter writer;

        bitwriter_init(
            &writer,
            output
        );


        while ((bytes_read =
                fread(
                    buffer,
                    1,
                    BUFFER_SIZE,
                    input
                )) > 0) {

            size_t j;

            for (j = 0;
                 j < bytes_read;
                 j++) {

                write_code(
                    &writer,
                    &codes[
                        buffer[j]
                    ]
                );
            }
        }


        /*
         * Completar último byte.
         */

        bitwriter_flush(
            &writer
        );
    }


    fclose(input);

    fclose(output);

    free_tree(root);


    printf(
        "Comprimido: %s\n",
        output_filename
    );

    return 1;
}


/* ============================================================
   PROCESAR DIRECTORIO
   ============================================================ */

static void process_directory(
        const char *directory)
{
    DIR *dir;

    struct dirent *entry;

    char path[PATH_MAX];


    dir =
        opendir(
            directory
        );

    if (dir == NULL) {

        perror(directory);

        return;
    }


    while ((entry =
            readdir(dir)) != NULL) {

        struct stat st;

        size_t len;


        /*
         * Ignorar . y ..
         */

        if (strcmp(
                entry->d_name,
                "."
            ) == 0 ||

            strcmp(
                entry->d_name,
                ".."
            ) == 0) {

            continue;
        }


        /*
         * Ruta completa.
         */

        snprintf(
            path,
            sizeof(path),
            "%s/%s",
            directory,
            entry->d_name
        );


        if (stat(
                path,
                &st
            ) != 0) {

            perror(path);

            continue;
        }


        /*
         * Solo archivos normales.
         */

        if (!S_ISREG(st.st_mode))
            continue;


        /*
         * Solo archivos .txt
         */

        len = strlen(path);

        if (len < 4)
            continue;

        if (strcmp(
                path + len - 4,
                ".txt"
            ) != 0) {

            continue;
        }


        /*
         * Comprimir.
         */

        compress_file(
            path
        );
    }


    closedir(dir);
}


/* ============================================================
   MAIN
   ============================================================ */

int main(
        int argc,
        char *argv[])
{
    struct stat st;


    if (argc != 2) {

        fprintf(
            stderr,
            "Uso: %s <directorio>\n",
            argv[0]
        );

        return EXIT_FAILURE;
    }


    /*
     * Verificar directorio.
     */

    if (stat(
            argv[1],
            &st
        ) != 0) {

        perror(argv[1]);

        return EXIT_FAILURE;
    }


    if (!S_ISDIR(st.st_mode)) {

        fprintf(
            stderr,
            "Error: %s no es un directorio.\n",
            argv[1]
        );

        return EXIT_FAILURE;
    }


    /*
     * Procesar libros.
     */

    process_directory(
        argv[1]
    );


    printf(
        "\nProceso terminado.\n"
    );


    return EXIT_SUCCESS;
}
