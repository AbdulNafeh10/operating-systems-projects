#define _POSIX_C_SOURCE 200809L
#include "hash_functions.h"
#include <limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEEP 16
#define TABLE_SIZE 8192
#define MAX_THREADS 32

typedef struct {
    unsigned char bytes[KEEP];
    atomic_int matched;
    const char *password;
    const char *algorithm;
} Hash;

typedef struct Entry {
    Hash *hash;
    struct Entry *next;
} Entry;

typedef struct {
    char **words;
    size_t word_count;
    size_t next_word;
    Hash *hashes;
    size_t hash_count;
    Entry *table[TABLE_SIZE];
    pthread_mutex_t lock;
    atomic_int failed;
} Job;

typedef unsigned char *(*HashFunction)(unsigned char *, unsigned int);
static HashFunction functions[] = {
    calculate_md5, calculate_sha1, calculate_sha256, calculate_sha512
};
static const char *algorithms[] = {"MD5", "SHA1", "SHA256", "SHA512"};

static unsigned int bucket(const unsigned char *bytes) {
    unsigned int value = 0;
    for (int i = 0; i < 4; i++) value = (value << 8) | bytes[i];
    return value % TABLE_SIZE;
}

static int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int load_hashes(Job *job, const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) return 0;
    char *line = NULL;
    size_t length = 0;
    int ok = 1;
    while (getline(&line, &length, file) != -1) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!*line) continue;
        if (strlen(line) != KEEP * 2) { ok = 0; break; }
        Hash h = {0};
        for (int i = 0; i < KEEP; i++) {
            int high = hex_digit(line[2 * i]);
            int low = hex_digit(line[2 * i + 1]);
            if (high < 0 || low < 0) { ok = 0; break; }
            h.bytes[i] = (unsigned char)(high * 16 + low);
        }
        if (!ok) break;
        Hash *tmp = realloc(job->hashes, (job->hash_count + 1) * sizeof(*tmp));
        if (!tmp) { ok = 0; break; }
        job->hashes = tmp;
        job->hashes[job->hash_count] = h;
        atomic_init(&job->hashes[job->hash_count].matched, 0);
        job->hash_count++;
    }
    if (ferror(file) || fclose(file) != 0) ok = 0;
    free(line);
    if (!ok) return 0;

    // Build the table only after resizing the hash array is complete.
    for (size_t i = 0; i < job->hash_count; i++) {
        unsigned int index = bucket(job->hashes[i].bytes);
        Entry *entry = malloc(sizeof(*entry));
        if (!entry) return 0;
        entry->hash = &job->hashes[i];
        entry->next = job->table[index];
        job->table[index] = entry;
    }
    return 1;
}

static int load_words(Job *job, const char *path) {
    FILE *file = fopen(path, "r");
    if (!file) return 0;
    char *line = NULL;
    size_t length = 0;
    int ok = 1;
    while (getline(&line, &length, file) != -1) {
        line[strcspn(line, "\r\n")] = '\0';
        if (!*line) continue;
        if (strlen(line) > UINT_MAX) { ok = 0; break; }
        char *copy = strdup(line);
        if (!copy) { ok = 0; break; }
        char **tmp = realloc(job->words, (job->word_count + 1) * sizeof(*tmp));
        if (!tmp) {
            free(copy);
            ok = 0;
            break;
        }
        job->words = tmp;
        job->words[job->word_count++] = copy;
    }
    if (ferror(file) || fclose(file) != 0) ok = 0;
    free(line);
    return ok;
}

static void *worker(void *arg) {
    Job *job = arg;
    while (1) {
        pthread_mutex_lock(&job->lock);
        if (job->next_word >= job->word_count) {
            pthread_mutex_unlock(&job->lock);
            break;
        }
        char *word = job->words[job->next_word++];
        pthread_mutex_unlock(&job->lock);

        for (size_t a = 0; a < 4; a++) {
            unsigned char *digest = functions[a]((unsigned char *)word, (unsigned int)strlen(word));
            if (!digest) { atomic_store(&job->failed, 1); return NULL; }
            for (Entry *entry = job->table[bucket(digest)]; entry; entry = entry->next) {
                Hash *hash = entry->hash;
                if (memcmp(hash->bytes, digest, KEEP) == 0) {
                    int expected = 0;
                    // Only one thread records each match; duplicates remain separate entries.
                    if (atomic_compare_exchange_strong(&hash->matched, &expected, 1)) {
                        hash->password = word;
                        hash->algorithm = algorithms[a];
                    }
                }
            }
            free(digest);
        }
    }
    return NULL;
}

static void cleanup(Job *job) {
    for (size_t i = 0; i < job->word_count; i++) free(job->words[i]);
    free(job->words);
    free(job->hashes);
    for (int i = 0; i < TABLE_SIZE; i++) {
        Entry *entry = job->table[i];
        while (entry) {
            Entry *next = entry->next;
            free(entry);
            entry = next;
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 4 && argc != 5) {
        fprintf(stderr, "Usage: %s PASSWORDS HASHES OUTPUT [THREADS: 1-32]\n", argv[0]);
        return 1;
    }
    int workers = 4;
    if (argc == 5) {
        char *end;
        long count = strtol(argv[4], &end, 10);
        if (!*argv[4] || *end || count < 1 || count > MAX_THREADS) {
            fputs("Threads must be between 1 and 32\n", stderr);
            return 1;
        }
        workers = (int)count;
    }
    Job job = {0};
    if (!load_hashes(&job, argv[2]) || !load_words(&job, argv[1])) {
        fputs("Unable to load input or invalid 32-digit hex hash\n", stderr);
        cleanup(&job);
        return 1;
    }
    if (pthread_mutex_init(&job.lock, NULL) != 0) {
        cleanup(&job);
        return 1;
    }
    pthread_t threads[MAX_THREADS];
    int started = 0;
    for (int i = 0; i < workers; i++) {
        if (pthread_create(&threads[i], NULL, worker, &job) != 0) {
            atomic_store(&job.failed, 1);
            break;
        }
        started++;
    }
    for (int i = 0; i < started; i++) pthread_join(threads[i], NULL);

    int ok = !atomic_load(&job.failed);
    if (ok) {
        FILE *out = fopen(argv[3], "w");
        if (!out) ok = 0;
        else {
            for (size_t i = 0; i < job.hash_count; i++) {
                Hash *h = &job.hashes[i];
                if (h->password) {
                    if (fprintf(out, "%s:%s\n", h->password, h->algorithm) < 0) ok = 0;
                } else if (fputs("not found\n", out) == EOF) ok = 0;
            }
            if (fclose(out) != 0) ok = 0;
        }
    }
    if (!ok) fputs("Unable to complete hashing or write results\n", stderr);
    pthread_mutex_destroy(&job.lock);
    cleanup(&job);
    return ok ? 0 : 1;
}
