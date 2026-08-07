#pragma once
#include <sodium.h>
#include <stdbool.h>

#define MAX_TRUSTED_KEYS 8
#define MAX_HASHES 256

typedef struct {
    unsigned char key[crypto_sign_PUBLICKEYBYTES];
    char *owner;
    long long expiry;
} TrustedKey;

bool verify_plugin_signature(const char *so_path);
bool verify_plugin_hash(const char *so_path);
void verify_add_trusted_key(const unsigned char *key, const char *owner, long long expiry);
void verify_clear_trusted_keys(void);