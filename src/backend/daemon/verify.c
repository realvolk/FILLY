#include "verify.h"
#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const unsigned char public_key[crypto_sign_PUBLICKEYBYTES] = {
    0x2b, 0x0e, 0xf7, 0x23, 0x05, 0x58, 0x60, 0x52, 0xfc, 0xb1, 0xdd, 0x20, 0x00, 0x11, 0xf4, 0xe9, 0xe5, 0x6a, 0x87, 0x41, 0xe2, 0xb9, 0x2a, 0xd5, 0x12, 0x95, 0xf0, 0x23, 0x0b, 0xf6, 0xb9, 0x7f
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