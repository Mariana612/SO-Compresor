

#include <stdio.h> // Bien
#include <stdlib.h> // Bien


#include <stdint.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include "BitWriter.h"
#include "MD5Utils.h"
#define SYMBOLS 256
#define MAX_CODE_LENGTH 256
#define BUFFER_SIZE (1024 * 1024)
#define MAGIC "HUF2"

//==============================================================
// ESTRUCTURAS PARA EL ARBOL
//==============================================================


/*----------------------------------------------------------------
NODO - nodo del árbol Huffman
----------------------------------------------------------------*/

typedef struct HuffmanNode {
    unsigned char symbol;       // Asegurar de que sea un numero entre 0 y 255
    uint64_t frequency;         // Para frecuencias grandes
    struct HuffmanNode *left, *right;

} HuffmanNode;


/*----------------------------------------------------------------
MIN HEAP - heap mínimo para construir el árbol Huffman
----------------------------------------------------------------*/
typedef struct {
    HuffmanNode **nodes;
    int size;
    int capacity;

} MinHeap;

typedef struct {
    unsigned char bits[MAX_CODE_LENGTH];
    int length;
} HuffmanCode;


//==============================================================
// VERIFICACIONES
//==============================================================

// ES HOJA
static int is_leaf(HuffmanNode *node)
{
    return node != NULL &&
           node->left == NULL &&
           node->right == NULL;
}



// =============================================================
// CODIGO DE HUFFMAN
// =============================================================


/*----------------------------------------------------------------
LIMPIAR MEMORIA
----------------------------------------------------------------*/

// LIBERAR HEAP
static void destroy_heap(MinHeap *heap)
{
    if (heap == NULL) return;

    free(heap->nodes);
    free(heap);
}

// LIBERAR ARBOL
static void free_tree(HuffmanNode *root)
{
    if (root == NULL)
        return;

    free_tree(root->left);
    free_tree(root->right);
    free(root);
}


/*----------------------------------------------------------------
UTILIDADES
----------------------------------------------------------------*/

// CALCULAR FRECUENCIAS
static int count_frequencies( const char *filename, uint64_t frequencies[SYMBOLS], uint64_t *total_bytes)
{
    FILE *file;
    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;

    memset( frequencies, 0, sizeof(uint64_t) * SYMBOLS);

    *total_bytes = 0;
    file = fopen( filename, "rb" );

    if (file == NULL) {
        perror(filename);
        return 0;
    }

    while ((bytes_read = fread( buffer, 1, BUFFER_SIZE, file)) > 0) {
        size_t i;
        for (i = 0; i < bytes_read; i++) {
            frequencies[ buffer[i] ]++;
            (*total_bytes)++;
        }
    }

    if (ferror(file)) {
        fprintf( stderr, "Error leyendo %s\n", filename);
        fclose(file);
        return 0;
    }

    fclose(file);
    return 1;
}

// CAMBIAR NODOS
static void swap_nodes( HuffmanNode **a, HuffmanNode **b)
{
    HuffmanNode *t;

    t = *a;
    *a = *b;
    *b = t;
}


// SHIFT UP - Insertar nodo
static void heap_push(
        MinHeap *heap,
        HuffmanNode *node)
{
    int i;

    heap->nodes[heap->size] = node;
    i = heap->size;
    heap->size++;

    while (i > 0) {
        int parent = (i - 1) / 2;
        if (heap->nodes[parent]->frequency <= heap->nodes[i]->frequency) {
            break;
        }

        swap_nodes( &heap->nodes[parent], &heap->nodes[i]);
        i = parent;
    }
}

// SHIFT DOWN - Extraer cabeza
static HuffmanNode *heap_pop(
        MinHeap *heap)
{
    HuffmanNode *result;

    int i;

    if (heap->size == 0) return NULL;

    result = heap->nodes[0];
    heap->size--;

    if (heap->size == 0) return result;

    heap->nodes[0] = heap->nodes[heap->size];
    i = 0;

    while (1) {

        int left = 2 * i + 1;
        int right = 2 * i + 2;
        int smallest = i;

        if (left < heap->size && heap->nodes[left]->frequency < heap->nodes[smallest]->frequency) {
            smallest = left;
        }
        if (right < heap->size && heap->nodes[right]->frequency < heap->nodes[smallest]->frequency) {
            smallest = right;
        }
        if (smallest == i) break;

        swap_nodes( &heap->nodes[i], &heap->nodes[smallest]);
        i = smallest;
    }

    return result;
}


/*----------------------------------------------------------------
CREACION
----------------------------------------------------------------*/

// CREAR NODO
static HuffmanNode *create_node(
        unsigned char symbol,
        uint64_t frequency,
        HuffmanNode *left,
        HuffmanNode *right)
{
    HuffmanNode *node;

    node = malloc(sizeof(HuffmanNode));

    node->symbol = symbol;
    node->frequency = frequency;
    node->left = left;
    node->right = right;

    return node;
}


// CREAR HEAP
static MinHeap *create_heap(int capacity)
{
    MinHeap *heap;

    heap = malloc(sizeof(MinHeap));
    
    heap->nodes = malloc(sizeof(HuffmanNode *) * capacity);    
    heap->size = 0;
    heap->capacity = capacity;

    return heap;
}


// CREAR ARBOL
static HuffmanNode *build_tree( uint64_t frequencies[SYMBOLS]) // Dar tabla de frecuencias
{
    MinHeap *heap;
    heap = create_heap(SYMBOLS);

    int i;
    for (i = 0; i < SYMBOLS; i++) {

        if (frequencies[i] > 0) {
            HuffmanNode *node;
            node = create_node( (unsigned char)i, frequencies[i], NULL, NULL );

            heap_push( heap, node);
        }
    }

    // Construcción
    while (heap->size > 1) {

        HuffmanNode *left = heap_pop(heap);
        HuffmanNode *right = heap_pop(heap);
        HuffmanNode *parent = create_node( 0, left->frequency + right->frequency, left, right );

        heap_push( heap, parent);
    }

    HuffmanNode *root = heap_pop(heap);
    destroy_heap(heap);

    return root;
}

/*----------------------------------------------------------------
CODIFICAR
----------------------------------------------------------------*/

static void compress_code( HuffmanNode *node, unsigned char path[], int depth, HuffmanCode codes[SYMBOLS])
{
    if (node == NULL)
        return;

    if (is_leaf(node)) {
        memcpy( codes[node->symbol].bits, path, (size_t)depth);
        codes[node->symbol].length = depth;
        return;
    }

    // Izquierda = 0
    path[depth] = 0;
    compress_code( node->left, path, depth + 1, codes);

    // Derecha = 1
    path[depth] = 1;
    compress_code( node->right, path, depth + 1,codes);
}


static void generate_codes( HuffmanNode *root, HuffmanCode codes[SYMBOLS])
{
    unsigned char path[MAX_CODE_LENGTH];
    memset(codes, 0, sizeof(HuffmanCode) * SYMBOLS );
    compress_code(root,path, 0, codes );
}


//little endian
static int write_uint64( FILE *file, uint64_t value)
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
                    codes[buffer[j]].bits,
                    codes[buffer[j]].length
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
   LEER UINT64
   ============================================================ */

static int read_uint64(
        FILE *file,
        uint64_t *value)
{
    int i;
    int byte;
    uint64_t result = 0;

    for (i = 0; i < 8; i++) {

        byte = fgetc(file);

        if (byte == EOF)
            return 0;

        result |=
            ((uint64_t)(unsigned char)byte)
            << (8 * i);
    }

    *value = result;

    return 1;
}


/* ============================================================
   BIT READER
   ============================================================ */

typedef struct {

    FILE *file;

    unsigned char buffer;

    int bits;

} BitReader;


static void bitreader_init(
        BitReader *reader,
        FILE *file)
{
    reader->file = file;

    reader->buffer = 0;

    reader->bits = 0;
}


static int read_bit(
        BitReader *reader)
{
    int bit;

    if (reader->bits == 0) {

        int value = fgetc(
            reader->file
        );

        if (value == EOF)
            return -1;

        reader->buffer =
            (unsigned char)value;

        reader->bits = 8;
    }

    bit =
        (reader->buffer >> 7) & 1;

    reader->buffer <<= 1;

    reader->bits--;

    return bit;
}


/* ============================================================
   DESCOMPRIMIR
   ============================================================ */

static int decompress_file(
        const char *compressed_filename,
        const char *output_directory)
{
    FILE *input;
    FILE *output;

    char output_filename[PATH_MAX];

    uint64_t frequencies[SYMBOLS];

    uint64_t original_size;

    uint64_t bytes_written = 0;

    unsigned char expected_md5[MD5_DIGEST_LENGTH];

    char magic[5];

    HuffmanNode *root;

    HuffmanNode *current;

    int i;


    /*
     * ========================================================
     * Abrir archivo comprimido
     * ========================================================
     */

    input = fopen(
        compressed_filename,
        "rb"
    );

    if (input == NULL) {

        perror(compressed_filename);

        return 0;
    }


    /*
     * ========================================================
     * Leer MAGIC
     * ========================================================
     */

    if (fread(
            magic,
            1,
            4,
            input
        ) != 4) {

        fprintf(
            stderr,
            "Error: archivo comprimido incompleto.\n"
        );

        fclose(input);

        return 0;
    }

    magic[4] = '\0';


    if (memcmp(
            magic,
            MAGIC,
            4
        ) != 0) {

        fprintf(
            stderr,
            "Error: formato de archivo no válido.\n"
        );

        fclose(input);

        return 0;
    }


    /*
     * ========================================================
     * Tamaño original
     * ========================================================
     */

    if (!read_uint64(
            input,
            &original_size)) {

        fprintf(
            stderr,
            "Error leyendo tamaño original.\n"
        );

        fclose(input);

        return 0;
    }


    /*
     * ========================================================
     * MD5 almacenado
     * ========================================================
     */

    if (fread(
            expected_md5,
            1,
            MD5_DIGEST_LENGTH,
            input
        ) != MD5_DIGEST_LENGTH) {

        fprintf(
            stderr,
            "Error leyendo MD5.\n"
        );

        fclose(input);

        return 0;
    }


    /*
     * ========================================================
     * Tabla de frecuencias
     * ========================================================
     */

    for (i = 0;
         i < SYMBOLS;
         i++) {

        if (!read_uint64(
                input,
                &frequencies[i])) {

            fprintf(
                stderr,
                "Error leyendo tabla de frecuencias.\n"
            );

            fclose(input);

            return 0;
        }
    }


    /*
     * ========================================================
     * Archivo vacío
     * ========================================================
     */

    if (original_size == 0) {

        const char *base;
        size_t len;


        /*
         * Obtener nombre sin .huff.
         */

        base =
            strrchr(
                compressed_filename,
                '/'
            );

        if (base == NULL)
            base = compressed_filename;
        else
            base++;


        len = strlen(base);


        if (len <= 5 ||
            strcmp(
                base + len - 5,
                ".huff"
            ) != 0) {

            fprintf(
                stderr,
                "Error: extensión .huff inválida.\n"
            );

            fclose(input);

            return 0;
        }


        snprintf(
            output_filename,
            sizeof(output_filename),
            "%s/%.*s",
            output_directory,
            (int)(len - 5),
            base
        );


        output = fopen(
            output_filename,
            "wb"
        );

        if (output == NULL) {

            perror(output_filename);

            fclose(input);

            return 0;
        }


        fclose(output);

        fclose(input);


        printf(
            "Archivo descomprimido: %s\n",
            output_filename
        );


        /*
         * Verificar MD5 incluso para archivos vacíos.
         */

        if (!verify_md5(
                output_filename,
                expected_md5)) {

            remove(output_filename);

            return 0;
        }


        return 1;
    }


    /*
     * ========================================================
     * Construir árbol Huffman
     * ========================================================
     */

    root =
        build_tree(
            frequencies
        );

    if (root == NULL) {

        fprintf(
            stderr,
            "Error: no se pudo reconstruir el árbol Huffman.\n"
        );

        fclose(input);

        return 0;
    }


    /*
     * ========================================================
     * Obtener nombre de salida
     * ========================================================
     */

    {
        const char *base;
        size_t len;


        base =
            strrchr(
                compressed_filename,
                '/'
            );

        if (base == NULL)
            base = compressed_filename;
        else
            base++;


        len = strlen(base);


        if (len <= 5 ||
            strcmp(
                base + len - 5,
                ".huff"
            ) != 0) {

            fprintf(
                stderr,
                "Error: el archivo debe terminar en .huff.\n"
            );

            free_tree(root);

            fclose(input);

            return 0;
        }


        snprintf(
            output_filename,
            sizeof(output_filename),
            "%s/%.*s",
            output_directory,
            (int)(len - 5),
            base
        );
    }


    /*
     * ========================================================
     * Crear archivo de salida
     * ========================================================
     */

    output =
        fopen(
            output_filename,
            "wb"
        );

    if (output == NULL) {

        perror(output_filename);

        free_tree(root);

        fclose(input);

        return 0;
    }


    /*
     * ========================================================
     * DESCOMPRESIÓN
     * ========================================================
     */

    {
        BitReader reader;

        bitreader_init(
            &reader,
            input
        );


        current = root;


        /*
         * Recorrer exactamente original_size
         * símbolos.
         *
         * Esto es importante porque el último byte
         * comprimido puede contener bits de relleno.
         */

        while (bytes_written < original_size) {

            int bit;


            /*
             * Caso especial: solamente existe
             * un símbolo en el archivo.
             */

            if (is_leaf(root)) {

                if (fputc(
                        root->symbol,
                        output
                    ) == EOF) {

                    fprintf(
                        stderr,
                        "Error escribiendo archivo.\n"
                    );

                    fclose(output);
                    fclose(input);
                    free_tree(root);

                    remove(output_filename);

                    return 0;
                }

                bytes_written++;

                continue;
            }


            /*
             * Leer siguiente bit.
             */

            bit =
                read_bit(
                    &reader
                );


            if (bit < 0) {

                fprintf(
                    stderr,
                    "Error: datos comprimidos incompletos.\n"
                );

                fclose(output);
                fclose(input);
                free_tree(root);

                remove(output_filename);

                return 0;
            }


            if (bit == 0)
                current = current->left;
            else
                current = current->right;


            /*
             * Verificar árbol corrupto.
             */

            if (current == NULL) {

                fprintf(
                    stderr,
                    "Error: árbol Huffman inválido.\n"
                );

                fclose(output);
                fclose(input);
                free_tree(root);

                remove(output_filename);

                return 0;
            }


            /*
             * Llegamos a una hoja.
             */

            if (is_leaf(current)) {

                if (fputc(
                        current->symbol,
                        output
                    ) == EOF) {

                    fprintf(
                        stderr,
                        "Error escribiendo archivo.\n"
                    );

                    fclose(output);
                    fclose(input);
                    free_tree(root);

                    remove(output_filename);

                    return 0;
                }


                bytes_written++;

                current = root;
            }
        }
    }


    fclose(output);

    fclose(input);

    free_tree(root);


    printf(
        "Archivo descomprimido: %s\n",
        output_filename
    );


    /*
     * ========================================================
     * VERIFICACIÓN MD5
     * ========================================================
     */

    if (!verify_md5(
            output_filename,
            expected_md5)) {

        /*
         * Si el MD5 no coincide, el archivo generado
         * se elimina porque no podemos garantizar
         * que la descompresión sea correcta.
         */

        remove(output_filename);

        fprintf(
            stderr,
            "El archivo descomprimido fue eliminado "
            "porque la verificación falló.\n"
        );

        return 0;
    }


    return 1;
}



/* ============================================================
   MAIN
   ============================================================ */

int main(
        int argc,
        char *argv[])
{
    struct stat st;


    /*
     * ========================================================
     * COMPRESIÓN
     * ========================================================
     *
     * ./huffman c libros/
     */

    if (argc == 3 &&
        strcmp(argv[1], "c") == 0) {

        if (stat(
                argv[2],
                &st
            ) != 0) {

            perror(argv[2]);

            return EXIT_FAILURE;
        }


        if (!S_ISDIR(st.st_mode)) {

            fprintf(
                stderr,
                "Error: %s no es un directorio.\n",
                argv[2]
            );

            return EXIT_FAILURE;
        }


        process_directory(
            argv[2]
        );


        printf(
            "\nProceso de compresión terminado.\n"
        );


        return EXIT_SUCCESS;
    }


    /*
     * ========================================================
     * DESCOMPRESIÓN
     * ========================================================
     *
     * ./huffman d archivo.huff directorio/
     */

    if (argc == 4 &&
        strcmp(argv[1], "d") == 0) {

        /*
         * Verificar archivo comprimido.
         */

        if (stat(
                argv[2],
                &st
            ) != 0) {

            perror(argv[2]);

            return EXIT_FAILURE;
        }


        if (!S_ISREG(st.st_mode)) {

            fprintf(
                stderr,
                "Error: %s no es un archivo.\n",
                argv[2]
            );

            return EXIT_FAILURE;
        }


        /*
         * Verificar directorio de salida.
         */

        if (stat(
                argv[3],
                &st
            ) != 0) {

            perror(argv[3]);

            return EXIT_FAILURE;
        }


        if (!S_ISDIR(st.st_mode)) {

            fprintf(
                stderr,
                "Error: %s no es un directorio.\n",
                argv[3]
            );

            return EXIT_FAILURE;
        }


        /*
         * Descomprimir y verificar MD5.
         */

        if (!decompress_file(
                argv[2],
                argv[3]
            )) {

            fprintf(
                stderr,
                "\nLa descompresión NO fue verificada correctamente.\n"
            );

            return EXIT_FAILURE;
        }


        printf(
            "\nDescompresión y verificación terminadas correctamente.\n"
        );


        return EXIT_SUCCESS;
    }


    /*
     * ========================================================
     * USO INCORRECTO
     * ========================================================
     */

    fprintf(
        stderr,
        "Uso:\n"
        "  %s c <directorio>\n"
        "  %s d <archivo.huff> <directorio_salida>\n",
        argv[0],
        argv[0]
    );


    return EXIT_FAILURE;
}
