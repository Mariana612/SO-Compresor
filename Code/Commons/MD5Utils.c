#include "MD5Utils.h"
#include <openssl/evp.h>
#include <stdio.h>
#include <string.h>

#define MD5_BUFFER_SIZE (1024 * 1024)

int calculate_md5(const char *filename, unsigned char md5_out[MD5_DIGEST_LENGTH])
{
    FILE *file;
    unsigned char buffer[MD5_BUFFER_SIZE];
    size_t bytes_read;
    EVP_MD_CTX *md5;
    int ok = 0;

    file = fopen(filename, "rb");
    if (file == NULL) {
        perror(filename);
        return 0;
    }
    md5 = EVP_MD_CTX_new();
    if (md5 == NULL || EVP_DigestInit_ex(md5, EVP_md5(), NULL) != 1)
        goto done;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
        if (EVP_DigestUpdate(md5, buffer, bytes_read) != 1)
            goto done;
    if (ferror(file)) {
        fprintf(stderr, "Error leyendo %s\n", filename);
        goto done;
    }
    ok = EVP_DigestFinal_ex(md5, md5_out, NULL) == 1;
done:
    EVP_MD_CTX_free(md5);
    fclose(file);
    return ok;
}

void md5_to_hex(const unsigned char md5[MD5_DIGEST_LENGTH], char hex[33])
{
    static const char digits[] = "0123456789abcdef";
    int index;
    for (index = 0; index < MD5_DIGEST_LENGTH; index++) {
        hex[index * 2] = digits[(md5[index] >> 4) & 0x0F];
        hex[index * 2 + 1] = digits[md5[index] & 0x0F];
    }
    hex[32] = '\0';
}

/* Imprime una sola línea por archivo para que la salida de varios hilos o
 * procesos no se entremezcle. */
int verify_md5(const char *filename, const unsigned char expected_md5[MD5_DIGEST_LENGTH])
{
    unsigned char calculated_md5[MD5_DIGEST_LENGTH];
    char calculated_hex[33], expected_hex[33];
    if (!calculate_md5(filename, calculated_md5))
        return 0;
    md5_to_hex(calculated_md5, calculated_hex);
    md5_to_hex(expected_md5, expected_hex);
    if (memcmp(calculated_md5, expected_md5, MD5_DIGEST_LENGTH) != 0) {
        printf("ERROR MD5 %s: almacenado %s, calculado %s\n", filename, expected_hex,
               calculated_hex);
        return 0;
    }
    printf("MD5 verificado %s (%s)\n", filename, calculated_hex);
    return 1;
}
