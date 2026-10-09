#ifndef HASH_FUNCTIONS_H
#define HASH_FUNCTIONS_H

// Each function returns the first 16 digest bytes (allocated; caller frees).
unsigned char *calculate_md5(unsigned char *data, unsigned int length);
unsigned char *calculate_sha1(unsigned char *data, unsigned int length);
unsigned char *calculate_sha256(unsigned char *data, unsigned int length);
unsigned char *calculate_sha512(unsigned char *data, unsigned int length);

#endif
