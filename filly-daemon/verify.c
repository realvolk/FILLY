#include "verify.h"
#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const unsigned char public_key[crypto_sign_PUBLICKEYBYTES] = {
    0xbe, 0x6b, 0x03, 0x61, 0x43, 0x3b, 0x6a, 0x8b, 0x6d, 0x8c, 0x1d, 0x5f, 0x87, 0x19, 0x30, 0x97, 0xf6, 0xac, 0x85, 0x07, 0xd4, 0x09, 0xbd, 0x5f, 0x6c, 0xb9, 0xa2, 0x49, 0x18, 0x2c, 0xb0, 0x33
};

bool verify_plugin_signature(const char *so_path) {
    char sig_path[2056];
    snprintf(sig_path, sizeof(sig_path), "%s.sig", so_path);

    struct stat st;
    if (stat(sig_path, &st) != 0) return false;
    if (st.st_size != crypto_sign_BYTES) return false;

    FILE *sf = fopen(sig_path, "rb");
    if (!sf) return false;
    unsigned char sig[crypto_sign_BYTES];
    if (fread(sig, 1, crypto_sign_BYTES, sf) != crypto_sign_BYTES) {
        fclose(sf);
        return false;
    }
    fclose(sf);

    FILE *so = fopen(so_path, "rb");
    if (!so) return false;
    fseek(so, 0, SEEK_END);
    long sz = ftell(so);
    rewind(so);
    unsigned char *data = malloc(sz);
    if (!data) { fclose(so); return false; }
    if (fread(data, 1, sz, so) != (size_t)sz) {
        free(data);
        fclose(so);
        return false;
    }
    fclose(so);

    int ret = crypto_sign_verify_detached(sig, data, sz, public_key);
    free(data);
    return ret == 0;
}