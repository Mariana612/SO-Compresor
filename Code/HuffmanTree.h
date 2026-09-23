#ifndef HUFFMAN_TREE_H
#define HUFFMAN_TREE_H

#include <stdint.h>

#define HUFFMAN_SYMBOLS 256
#define HUFFMAN_MAX_CODE_LENGTH 256

typedef struct HuffmanNode {
    unsigned char symbol;
    uint64_t frequency;
    struct HuffmanNode *left;
    struct HuffmanNode *right;
} HuffmanNode;

typedef struct {
    unsigned char bits[HUFFMAN_MAX_CODE_LENGTH];
    int length;
} HuffmanCode;

HuffmanNode *huffman_build_tree(const uint64_t frequencies[HUFFMAN_SYMBOLS]);
void huffman_generate_codes(const HuffmanNode *root, HuffmanCode codes[HUFFMAN_SYMBOLS]);
int huffman_is_leaf(const HuffmanNode *node);
void huffman_free_tree(HuffmanNode *root);

#endif