#include "verify.h"
#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <strings.h>

static TrustedKey trusted_keys[MAX_TRUSTED_KEYS];
static int trusted_key_count = 0;

static const unsigned char builtin_key[crypto_sign_PUBLICKEYBYTES] = {
    0x2b, 0x0e, 0xf7, 0x23, 0x05, 0x58, 0x60, 0x52, 0xfc, 0xb1, 0xdd, 0x20, 0x00, 0x11, 0xf4, 0xe9,
    0xe5, 0x6a, 0x87, 0x41, 0xe2, 0xb9, 0x2a, 0xd5, 0x12, 0x95, 0xf0, 0x23, 0x0b, 0xf6, 0xb9, 0x7f
};

void verify_add_trusted_key(const unsigned char *key, const char *owner, long long expiry) {
    if (trusted_key_count >= MAX_TRUSTED_KEYS) return;
    memcpy(trusted_keys[trusted_key_count].key, key, crypto_sign_PUBLICKEYBYTES);
    trusted_keys[trusted_key_count].owner = owner ? strdup(owner) : strdup("unknown");
    trusted_keys[trusted_key_count].expiry = expiry;
    trusted_key_count++;
}

void verify_clear_trusted_keys(void) {
    for (int i = 0; i < trusted_key_count; i++)
        free(trusted_keys[i].owner);
    trusted_key_count = 0;
}

static bool check_key_expired(TrustedKey *tk) {
    if (tk->expiry <= 0) return false;
    return time(NULL) > tk->expiry;
}

bool verify_plugin_signature(const char *so_path) {
    char sig_path[2056];
    snprintf(sig_path, sizeof(sig_path), "%s.sig", so_path);

    struct stat st;
    if (stat(sig_path, &st) != 0) return false;
    if (st.st_size > crypto_sign_BYTES * MAX_TRUSTED_KEYS) return false;

    unsigned char *sig_data = malloc(st.st_size);
    if (!sig_data) return false;

    FILE *sf = fopen(sig_path, "rb");
    if (!sf) { free(sig_data); return false; }
    size_t sig_size = fread(sig_data, 1, st.st_size, sf);
    fclose(sf);

    FILE *so = fopen(so_path, "rb");
    if (!so) { free(sig_data); return false; }
    fseek(so, 0, SEEK_END);
    long sz = ftell(so);
    rewind(so);
    unsigned char *data = malloc(sz);
    if (!data) { fclose(so); free(sig_data); return false; }
    if (fread(data, 1, sz, so) != (size_t)sz) {
        free(data); fclose(so); free(sig_data);
        return false;
    }
    fclose(so);

    bool verified = false;

    if (sig_size >= crypto_sign_BYTES) {
        if (crypto_sign_verify_detached(sig_data, data, sz, builtin_key) == 0)
            verified = true;
    }

    for (int i = 0; i < trusted_key_count && !verified; i++) {
        if (check_key_expired(&trusted_keys[i])) continue;
        if (sig_size >= crypto_sign_BYTES * (i + 2)) {
            if (crypto_sign_verify_detached(sig_data + crypto_sign_BYTES * (i + 1), data, sz, trusted_keys[i].key) == 0)
                verified = true;
        }
    }

    free(data);
    free(sig_data);
    return verified;
}

bool verify_plugin_hash(const char *so_path) {
    char hash_path[2056];
    snprintf(hash_path, sizeof(hash_path), "%s.sha256", so_path);

    struct stat st;
    if (stat(hash_path, &st) != 0) return false;

    FILE *hf = fopen(hash_path, "r");
    if (!hf) return false;

    char expected_hex[65] = {0};
    if (fread(expected_hex, 1, 64, hf) < 64) { fclose(hf); return false; }
    fclose(hf);

    FILE *so = fopen(so_path, "rb");
    if (!so) return false;
    fseek(so, 0, SEEK_END);
    long sz = ftell(so);
    rewind(so);
    unsigned char *data = malloc(sz);
    if (!data) { fclose(so); return false; }
    fread(data, 1, sz, so);
    fclose(so);

    unsigned char hash[crypto_hash_sha256_BYTES];
    crypto_hash_sha256(hash, data, sz);
    free(data);

    char actual_hex[65];
    sodium_bin2hex(actual_hex, sizeof(actual_hex), hash, crypto_hash_sha256_BYTES);

    return strncasecmp(expected_hex, actual_hex, 64) == 0;
}