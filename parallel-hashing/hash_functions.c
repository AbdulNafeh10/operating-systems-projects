#include "hash_functions.h"
#include <openssl/evp.h>
#include <stdlib.h>
#include <string.h>

static unsigned char *digest(const EVP_MD *algorithm, unsigned char *data,
                             unsigned int length) {
    unsigned char full[EVP_MAX_MD_SIZE];
    unsigned int full_length;
    if (!EVP_Digest(data, length, full, &full_length, algorithm, NULL) || full_length < 16)
        return NULL;
    unsigned char *first = malloc(16);
    if (first) memcpy(first, full, 16);
    return first;
}

unsigned char *calculate_md5(unsigned char *data, unsigned int size) {
    return digest(EVP_md5(), data, size);
}
unsigned char *calculate_sha1(unsigned char *data, unsigned int size) {
    return digest(EVP_sha1(), data, size);
}
unsigned char *calculate_sha256(unsigned char *data, unsigned int size) {
    return digest(EVP_sha256(), data, size);
}
unsigned char *calculate_sha512(unsigned char *data, unsigned int size) {
    return digest(EVP_sha512(), data, size);
}
