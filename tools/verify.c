#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>

static int hex_to_bytes(const char *hex, unsigned char *out, size_t out_len) {
    for (size_t i = 0; i < out_len; i++) {
        unsigned int byte;
        if (sscanf(hex + i * 2, "%2x", &byte) != 1) return 0;
        out[i] = (unsigned char)byte;
    }
    return 1;
}

static int read_file(const char *path, unsigned char **data, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return -1; }
    rewind(f);
    *data = malloc(sz);
    if (!*data) { fclose(f); return -1; }
    if (fread(*data, 1, sz, f) != (size_t)sz) { free(*data); fclose(f); return -1; }
    fclose(f);
    *size = sz;
    return 0;
}

static void print_usage(void) {
    fprintf(stderr, "Usage: verify <plugin.so> <plugin.so.sig> [public_key.pub]\n");
    fprintf(stderr, "Verifies a FILLY plugin signature.\n");
    fprintf(stderr, "  public_key.pub  Path to public key (default: tools/filly-key.pub,\n");
    fprintf(stderr, "                  relative to verify binary)\n");
}

int main(int argc, char **argv) {
    if (argc < 3) { print_usage(); return 1; }

    if (sodium_init() < 0) { fprintf(stderr, "sodium_init failed\n"); return 1; }

    const char *pubkeyfile = NULL;
    if (argc >= 4) {
        pubkeyfile = argv[3];
    } else {
        static char default_pub[1024];
        char self_path[1024];
        if (readlink("/proc/self/exe", self_path, sizeof(self_path) - 1) > 0) {
            snprintf(default_pub, sizeof(default_pub), "%s/filly-key.pub", dirname(self_path));
        } else {
            snprintf(default_pub, sizeof(default_pub), "tools/filly-key.pub");
        }
        pubkeyfile = default_pub;
    }

    unsigned char *pk_hex = NULL;
    size_t pk_hex_len = 0;
    if (read_file(pubkeyfile, &pk_hex, &pk_hex_len) < 0) {
        fprintf(stderr, "Cannot read public key from %s\n", pubkeyfile);
        return 1;
    }
    if (pk_hex_len > 0 && pk_hex[pk_hex_len - 1] == '\n') pk_hex[--pk_hex_len] = '\0';
    if (pk_hex_len != crypto_sign_PUBLICKEYBYTES * 2) {
        fprintf(stderr, "Invalid public key length\n");
        free(pk_hex);
        return 1;
    }
    unsigned char pk[crypto_sign_PUBLICKEYBYTES];
    if (!hex_to_bytes((char *)pk_hex, pk, crypto_sign_PUBLICKEYBYTES)) {
        fprintf(stderr, "Invalid hex in public key\n");
        free(pk_hex);
        return 1;
    }
    free(pk_hex);

    unsigned char *plugin_data = NULL;
    size_t plugin_size = 0;
    if (read_file(argv[1], &plugin_data, &plugin_size) < 0) {
        perror("Cannot read plugin file");
        return 1;
    }

    unsigned char *sig_data = NULL;
    size_t sig_size = 0;
    if (read_file(argv[2], &sig_data, &sig_size) < 0) {
        perror("Cannot read signature file");
        free(plugin_data);
        return 1;
    }
    if (sig_size != crypto_sign_BYTES) {
        fprintf(stderr, "Invalid signature size: expected %d, got %zu\n", crypto_sign_BYTES, sig_size);
        free(plugin_data); free(sig_data);
        return 1;
    }

    int ret = crypto_sign_verify_detached(sig_data, plugin_data, plugin_size, pk);
    free(plugin_data); free(sig_data);

    if (ret == 0) {
        printf("VERIFIED: %s\n", argv[1]);
        return 0;
    } else {
        printf("INVALID signature: %s\n", argv[1]);
        return 1;
    }
}