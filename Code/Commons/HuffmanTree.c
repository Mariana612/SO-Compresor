#include "HuffmanTree.h"
#include <stdlib.h>
#include <string.h>

// VERIFICACIONES ---------------------------------------------

// Es Hoja -  Verificación de nodo hoja
int is_leaf(const HuffmanNode *node) 
{
    return node != NULL && node->left == NULL && node->right == NULL;
}

// Excepcion de limpieza - Limpieza de nodos pendientes en caso de error
static void clean_tree_exception(HuffmanNode *pending[], int count)
{
    int i;
    for (i = 0; i < count; i++)
        clean_tree(pending[i]);
}


// FUNCIONES AUXILIARES ---------------------------------------


// Nuevo nodo - Asignación de memoria y valores
static HuffmanNode *new_node(unsigned char symbol, uint64_t frequency,
                             HuffmanNode *left, HuffmanNode *right)
{
    HuffmanNode *node = malloc(sizeof(HuffmanNode));
    if (node == NULL)
        return NULL;

    node->symbol = symbol;
    node->frequency = frequency;
    node->left = left;
    node->right = right;
    return node;
}

// Limpiar arbol - Liberación de memoria del árbol
void clean_tree(HuffmanNode *root)
{
    if (root == NULL)
        return;

    clean_tree(root->left);
    clean_tree(root->right);
    free(root);
}

// Quitar pequeño - Remueve el nodo con menor frecuencia de la lista de pendientes
static HuffmanNode *remove_smallest(HuffmanNode *pending[], int *count)
{
    HuffmanNode *smallest_node;
    int smallest = 0;
    int i;

    for (i = 1; i < *count; i++) {
        if (pending[i]->frequency < pending[smallest]->frequency)
            smallest = i;
    }

    smallest_node = pending[smallest];
    pending[smallest] = pending[*count - 1];
    (*count)--;
    return smallest_node;
}



// FUNCIONES PRINCIPALES ---------------------------------------


// Construir árbol de Huffman - Construye el árbol a partir de las frecuencias
HuffmanNode *huffman_build_tree(const uint64_t frequencies[HUFFMAN_SYMBOLS])
{
    HuffmanNode *pending[HUFFMAN_SYMBOLS]; 
    int count = 0;
    int symbol;

    // Nodos hoja para cada símbolo con frecuencia > 0
    for (symbol = 0; symbol < HUFFMAN_SYMBOLS; symbol++) {
        if (frequencies[symbol] == 0)
            continue;

        pending[count] = new_node((unsigned char)symbol, frequencies[symbol], NULL, NULL);
        if (pending[count] == NULL) {
            clean_tree_exception(pending, count);
            return NULL;
        }
        count++;
    }

    if (count == 0)
        return NULL;

    // Mezcla de Nodos - Combina los nodos hasta que quede solo la raiz
    while (count > 1) {
        HuffmanNode *first = remove_smallest(pending, &count);
        HuffmanNode *second = remove_smallest(pending, &count);
        HuffmanNode *parent = new_node(0, first->frequency + second->frequency, first, second);

        if (parent == NULL) {
            clean_tree(first);
            clean_tree(second);
            clean_tree_exception(pending, count);
            return NULL;
        }

        pending[count] = parent;
        count++;
    }


    return pending[0]; // Raíz del árbol
}

// Generar códigos - Genera los códigos binarios para cada símbolo a partir del árbol
static void save_codes(const HuffmanNode *node, unsigned char path[], int depth,
                       HuffmanCode codes[HUFFMAN_SYMBOLS])
{
    if (node == NULL)
        return;

    if (is_leaf(node)) {
        memcpy(codes[node->symbol].bits, path, depth);
        codes[node->symbol].length = depth;
        return;
    }

    path[depth] = 0; // Camino izquierdo
    save_codes(node->left, path, depth + 1, codes);

    path[depth] = 1; // Camino derecho
    save_codes(node->right, path, depth + 1, codes);
}



void huffman_generate_codes(const HuffmanNode *root, HuffmanCode codes[HUFFMAN_SYMBOLS])
{
    unsigned char path[HUFFMAN_MAX_CODE_LENGTH]; // Path temp

    memset(codes, 0, sizeof(HuffmanCode) * HUFFMAN_SYMBOLS);
    save_codes(root, path, 0, codes);
}
