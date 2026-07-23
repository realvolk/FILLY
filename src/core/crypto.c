#include "crypto.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <crypt.h>
#include <unistd.h>
#include <sys/random.h>

char *filly_hash_password(const char *password) {
    if (!password || !*password) return strdup("");

    char salt[32];
    unsigned char rand_bytes[16];
    if (getrandom(rand_bytes, sizeof(rand_bytes), 0) < 0) {
        for (int i = 0; i < 16; i++) rand_bytes[i] = rand() & 0xFF;
    }

    snprintf(salt, sizeof(salt), "$6$%02x%02x%02x%02x%02x%02x%02x%02x",
             rand_bytes[0], rand_bytes[1], rand_bytes[2], rand_bytes[3],
             rand_bytes[4], rand_bytes[5], rand_bytes[6], rand_bytes[7]);

    struct crypt_data data;
    memset(&data, 0, sizeof(data));
    char *hash = crypt_r(password, salt, &data);
    if (!hash) return strdup("");

    return strdup(hash);
}