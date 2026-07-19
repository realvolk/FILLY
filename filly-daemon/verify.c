#include "verify.h"
#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const unsigned char public_key[crypto_sign_PUBLICKEYBYTES] = {
    0x98, 0xa4, 0x1b, 0x27, 0xea, 0x66, 0xf1, 0xb7, 0x2d, 0x5a, 0x92, 0x2a, 0x86, 0x0c, 0xc7, 0x05, 0x04, 0x3d, 0x12, 0x96, 0xd1, 0xe2, 0xfa, 0xf0, 0x28, 0xfb, 0xe9, 0x63, 0x4e, 0x34, 0x16, 0xb6
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