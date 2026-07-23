#include "verify.h"
#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const unsigned char public_key[crypto_sign_PUBLICKEYBYTES] = {
    0x50, 0x2e, 0xea, 0x19, 0x27, 0x97, 0xed, 0xfd, 0x74, 0xb5, 0x95, 0xc0, 0x1b, 0x97, 0x1d, 0x4c, 0x13, 0xef, 0x62, 0x97, 0xa1, 0x0a, 0xb4, 0xad, 0xd1, 0xcd, 0x05, 0x61, 0x3a, 0xa2, 0x2d, 0x13
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