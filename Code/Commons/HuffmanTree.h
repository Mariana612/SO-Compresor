#ifndef COMMONS_HUFFMAN_TREE_H
#define COMMONS_HUFFMAN_TREE_H

#include <stdint.h>

/* Cantidad de símbolos posibles: un byte puede valer de 0 a 255. */
#define HUFFMAN_SYMBOLS 256

/* Longitud máxima de un código. Con 256 símbolos el árbol nunca tiene más de
 * 255 niveles, así que 256 alcanza. */
#define HUFFMAN_MAX_CODE_LENGTH 256

/* Nodo del árbol de Huffman. Las hojas guardan un símbolo; los nodos internos
 * solo sirven para unir dos subárboles. */
typedef struct HuffmanNode {
    unsigned char symbol;       /* byte que representa (solo válido en hojas) */
    uint64_t frequency;         /* veces que aparece el símbolo o suma de sus hijos */
    struct HuffmanNode *left;   /* camino con bit 0 */
    struct HuffmanNode *right;  /* camino con bit 1 */
} HuffmanNode;

/* Código binario de un símbolo: bits[0..length-1] valen 0 o 1. */
typedef struct {
    unsigned char bits[HUFFMAN_MAX_CODE_LENGTH];
    int length;
} HuffmanCode;

HuffmanNode *huffman_build_tree(const uint64_t frequencies[HUFFMAN_SYMBOLS]);
void huffman_generate_codes(const HuffmanNode *root, HuffmanCode codes[HUFFMAN_SYMBOLS]);
int is_leaf(const HuffmanNode *node);
void clean_tree(HuffmanNode *root);

#endif
