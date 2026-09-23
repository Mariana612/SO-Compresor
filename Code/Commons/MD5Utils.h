#ifndef COMMONS_MD5_UTILS_H
#define COMMONS_MD5_UTILS_H

#include <openssl/md5.h>

int calculate_md5(const char *filename, unsigned char md5_out[MD5_DIGEST_LENGTH]);
int verify_md5(const char *filename, const unsigned char expected_md5[MD5_DIGEST_LENGTH]);

#endif
