#include "HuffmanTree.h"
#include <stdlib.h>
#include <string.h>



// ESTRUCTURAS DE DATOS ---------------------------------------
typedef struct {
    HuffmanNode **nodes;
    int size;
} MinHeap;

// FUNCIONES AUXILIARES ---------------------------------------
int huffman_is_leaf(const HuffmanNode *node)
{
    return node != NULL && node->left == NULL && node->right == NULL;
}

static HuffmanNode *create_node(unsigned char symbol, uint64_t frequency,
                                HuffmanNode *left, HuffmanNode *right)
{
    HuffmanNode *node = malloc(sizeof(*node));

    if (node == NULL)
        return NULL;

    node->symbol = symbol;
    node->frequency = frequency;
    node->left = left;
    node->right = right;
    return node;
}

static void swap_nodes(HuffmanNode **first, HuffmanNode **second)
{
    HuffmanNode *temporary = *first;
    *first = *second;
    *second = temporary;
}

static int heap_push(MinHeap *heap, HuffmanNode *node)
{
    int index = heap->size++;

    heap->nodes[index] = node;
    while (index > 0) {
        int parent = (index - 1) / 2;
        if (heap->nodes[parent]->frequency <= heap->nodes[index]->frequency)
            break;
        swap_nodes(&heap->nodes[parent], &heap->nodes[index]);
        index = parent;
    }
    return 1;
}

static HuffmanNode *heap_pop(MinHeap *heap)
{
    HuffmanNode *result;
    int index = 0;

    if (heap->size == 0)
        return NULL;

    result = heap->nodes[0];
    heap->size--;
    if (heap->size == 0)
        return result;

    heap->nodes[0] = heap->nodes[heap->size];
    while (1) {
        int left = 2 * index + 1;
        int right = 2 * index + 2;
        int smallest = index;

        if (left < heap->size && heap->nodes[left]->frequency < heap->nodes[smallest]->frequency)
            smallest = left;
        if (right < heap->size && heap->nodes[right]->frequency < heap->nodes[smallest]->frequency)
            smallest = right;
        if (smallest == index)
            break;
        swap_nodes(&heap->nodes[index], &heap->nodes[smallest]);
        index = smallest;
    }
    return result;
}

void huffman_free_tree(HuffmanNode *root)
{
    if (root == NULL)
        return;
    huffman_free_tree(root->left);
    huffman_free_tree(root->right);
    free(root);
}

// FUNCIONES PRINCIPALES ---------------------------------------
HuffmanNode *huffman_build_tree(const uint64_t frequencies[HUFFMAN_SYMBOLS])
{
    MinHeap heap = { malloc(sizeof(*heap.nodes) * HUFFMAN_SYMBOLS), 0 };
    HuffmanNode *root;
    int symbol;

    if (heap.nodes == NULL)
        return NULL;

    for (symbol = 0; symbol < HUFFMAN_SYMBOLS; symbol++) {
        if (frequencies[symbol] > 0) {
            HuffmanNode *node = create_node((unsigned char)symbol, frequencies[symbol], NULL, NULL);
            if (node == NULL) {
                free(heap.nodes);
                return NULL;
            }
            heap_push(&heap, node);
        }
    }

    while (heap.size > 1) {
        HuffmanNode *left = heap_pop(&heap);
        HuffmanNode *right = heap_pop(&heap);
        HuffmanNode *parent = create_node(0, left->frequency + right->frequency, left, right);
        if (parent == NULL) {
            huffman_free_tree(left);
            huffman_free_tree(right);
            free(heap.nodes);
            return NULL;
        }
        heap_push(&heap, parent);
    }

    root = heap_pop(&heap);
    free(heap.nodes);
    return root;
}

static void generate_codes(const HuffmanNode *node, unsigned char path[], int depth,
                           HuffmanCode codes[HUFFMAN_SYMBOLS])
{
    if (node == NULL)
        return;
    if (huffman_is_leaf(node)) {
        memcpy(codes[node->symbol].bits, path, (size_t)depth);
        codes[node->symbol].length = depth;
        return;
    }
    path[depth] = 0;
    generate_codes(node->left, path, depth + 1, codes);
    path[depth] = 1;
    generate_codes(node->right, path, depth + 1, codes);
}

void huffman_generate_codes(const HuffmanNode *root, HuffmanCode codes[HUFFMAN_SYMBOLS])
{
    unsigned char path[HUFFMAN_MAX_CODE_LENGTH];

    memset(codes, 0, sizeof(HuffmanCode) * HUFFMAN_SYMBOLS);
    generate_codes(root, path, 0, codes);
}