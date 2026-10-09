#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int number(const char *text, size_t *result) {
    if (!*text || *text == '-') return 0;
    errno = 0;
    char *end;
    unsigned long long value = strtoull(text, &end, 10);
    if (errno || *end || value == 0 || value > SIZE_MAX) return 0;
    *result = (size_t)value;
    return 1;
}

static int read_hex(const char *path, unsigned char *bytes, size_t size) {
    FILE *file = fopen(path, "r");
    if (!file) return 0;
    size_t count = 0;
    int high = -1, ch, ok = 1;
    while ((ch = fgetc(file)) != EOF) {
        if (isspace((unsigned char)ch)) continue;
        int digit = ch >= '0' && ch <= '9' ? ch - '0' :
                    ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 :
                    ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1;
        if (digit < 0) { ok = 0; break; }
        if (high < 0) high = digit;
        else {
            if (count == size) { ok = 0; break; }
            bytes[count++] = (unsigned char)(high * 16 + digit);
            high = -1;
        }
    }
    if (ferror(file) || high >= 0 || count != size) ok = 0;
    if (fclose(file) != 0) ok = 0;
    return ok;
}

int main(int argc, char **argv) {
    if (argc < 8) {
        fprintf(stderr, "Usage: %s BLOCK_SIZE FILE_BYTES INPUT_HEX DISK_CAPACITY DISK1 DISK2 DISK3 [DISK...]\n", argv[0]);
        return 1;
    }
    size_t block, size, capacity;
    if (!number(argv[1], &block) || !number(argv[2], &size) ||
        !number(argv[4], &capacity)) {
        fputs("Invalid block size, file size, or disk capacity\n", stderr);
        return 1;
    }
    size_t disks = (size_t)argc - 5;
    size_t data_blocks = size / block + (size % block != 0);
    size_t stripes = data_blocks / (disks - 1) + (data_blocks % (disks - 1) != 0);
    if (stripes > capacity / block) {
        fputs("Not enough disk capacity\n", stderr);
        return 1;
    }

    unsigned char *source = malloc(size);
    unsigned char **disk = calloc(disks, sizeof(*disk));
    if (!source || !disk) { fputs("Out of memory\n", stderr); free(source); free(disk); return 1; }
    if (!read_hex(argv[3], source, size)) {
        fputs("Input must contain exactly FILE_BYTES valid hex bytes\n", stderr);
        free(source); free(disk); return 1;
    }
    int ok = 1;
    for (size_t d = 0; d < disks; d++) {
        disk[d] = calloc(capacity, 1);
        if (!disk[d]) { ok = 0; break; }
    }

    if (ok) {
        size_t offset = 0;
        for (size_t stripe = 0; stripe < stripes; stripe++) {
            size_t parity = disks - 1 - (stripe % disks);
            size_t pos = stripe * block;
            for (size_t i = 1; i < disks; i++) {
                size_t d = (parity + i) % disks;
                size_t remaining = size - offset;
                size_t chunk = remaining < block ? remaining : block;
                if (chunk) memcpy(disk[d] + pos, source + offset, chunk);
                offset += chunk;
            }
            // XOR of the data blocks reconstructs the parity block for each stripe.
            for (size_t i = 0; i < disks; i++) {
                if (i == parity) continue;
                for (size_t b = 0; b < block; b++) disk[parity][pos + b] ^= disk[i][pos + b];
            }
        }
        for (size_t d = 0; d < disks; d++) {
            FILE *out = fopen(argv[5 + d], "w");
            if (!out) { ok = 0; break; }
            for (size_t b = 0; b < capacity; b++) {
                if (fprintf(out, "%02x", disk[d][b]) < 0) { ok = 0; break; }
            }
            if (fclose(out) != 0) ok = 0;
            if (!ok) break;
        }
    }
    if (!ok) fputs("Unable to allocate or write disk files\n", stderr);
    for (size_t d = 0; d < disks; d++) free(disk[d]);
    free(disk);
    free(source);
    return ok ? 0 : 1;
}
