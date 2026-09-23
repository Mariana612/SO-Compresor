#include "MD5Utils.h"
#include <openssl/evp.h>
#include <stdio.h>
#include <string.h>

#define MD5_BLOCK_SIZE 65536   // Bytes que se leen del archivo en cada vuelta

// Calcular MD5 - Calcula la firma MD5 de un archivo usando OpenSSL
int calculate_md5(const char *filename, unsigned char md5_out[MD5_DIGEST_LENGTH])
{
    unsigned char block[MD5_BLOCK_SIZE];
    size_t bytes_read;
    EVP_MD_CTX *context;
    FILE *file = fopen(filename, "rb");

    if (file == NULL) {
        fprintf(stderr, "Error: no se pudo abrir %s\n", filename);
        return 0;
    }

    // El "contexto" guarda el cálculo del MD5 mientras se va leyendo el archivo
    context = EVP_MD_CTX_new();
    if (context == NULL) {
        fclose(file);
        return 0;
    }
    EVP_DigestInit_ex(context, EVP_md5(), NULL);

    // Se le pasa el archivo al MD5 por bloques
    while ((bytes_read = fread(block, 1, MD5_BLOCK_SIZE, file)) > 0)
        EVP_DigestUpdate(context, block, bytes_read);

    // Al final se obtiene la firma de 16 bytes
    EVP_DigestFinal_ex(context, md5_out, NULL);

    EVP_MD_CTX_free(context);
    fclose(file);
    return 1;
}

// Verificar MD5 - Calcula el MD5 del archivo y lo compara con el guardado
int verify_md5(const char *filename, const unsigned char expected_md5[MD5_DIGEST_LENGTH])
{
    unsigned char calculated_md5[MD5_DIGEST_LENGTH];

    if (!calculate_md5(filename, calculated_md5))
        return 0;
    return memcmp(calculated_md5, expected_md5, MD5_DIGEST_LENGTH) == 0;
}
