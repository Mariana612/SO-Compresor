#include "MD5Utils.h"

#include <stdio.h>
#include <string.h>

#define MD5_BUFFER_SIZE (1024 * 1024)

int calculate_md5( const char *filename, unsigned char md5_out[MD5_DIGEST_LENGTH])
{
    FILE *file;
    unsigned char buffer[MD5_BUFFER_SIZE];
    size_t bytes_read;
    MD5_CTX md5;

    file = fopen(filename, "rb");
    if (file == NULL) {
        perror(filename);
        return 0;
    }

    MD5_Init(&md5);

    while ((bytes_read = fread(buffer, 1, MD5_BUFFER_SIZE, file)) > 0)
        MD5_Update(&md5, buffer, bytes_read);

    if (ferror(file)) {
        fprintf(stderr, "Error leyendo %s\n", filename);
        fclose(file);
        return 0;
    }

    MD5_Final(md5_out, &md5);
    fclose(file);
    return 1;
}

// Preparar para texto
void md5_to_hex(
        const unsigned char md5[MD5_DIGEST_LENGTH],
        char hex[33])
{
    static const char digits[] = "0123456789abcdef";

    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        hex[i * 2] = digits[(md5[i] >> 4) & 0x0F];
        hex[i * 2 + 1] = digits[md5[i] & 0x0F];
    }

    hex[32] = '\0';
}

//Comprobar MD5
int verify_md5( const char *filename, const unsigned char expected_md5[MD5_DIGEST_LENGTH])
{
    unsigned char calculated_md5[MD5_DIGEST_LENGTH];
    char calculated_hex[33];
    char expected_hex[33];

    if (!calculate_md5(filename, calculated_md5))
        return 0;

    md5_to_hex(calculated_md5, calculated_hex);
    md5_to_hex(expected_md5, expected_hex);

    printf("MD5 almacenado : %s\n", expected_hex);
    printf("MD5 calculado  : %s\n", calculated_hex);

    if (memcmp(calculated_md5, expected_md5, MD5_DIGEST_LENGTH) != 0) {
        printf("ERROR: la verificación MD5 FALLÓ.\n");
        return 0;
    }

    printf("MD5 verificado correctamente.\n");
    return 1;
}
