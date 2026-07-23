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
    fprintf(stderr, "Usage: sign <plugin.so> [output.sig]\n");
    fprintf(stderr, "Signs a FILLY plugin .so file with Ed25519.\n");
    fprintf(stderr, "  plugin.so     The shared object to sign\n");
    fprintf(stderr, "  output.sig    Output signature file (default: plugin.so.sig)\n");
    fprintf(stderr, "\nEnvironment:\n");
    fprintf(stderr, "  FILLY_SIGN_KEY  Path to secret key file (hex-encoded)\n");
    fprintf(stderr, "                  Default: tools/filly-key.sec (relative to sign binary)\n");
}

int main(int argc, char **argv) {
    if (argc < 2) { print_usage(); return 1; }
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) { print_usage(); return 0; }

    if (sodium_init() < 0) { fprintf(stderr, "sodium_init failed\n"); return 1; }

    const char *keyfile = getenv("FILLY_SIGN_KEY");
    if (!keyfile) {
        static char default_key[1024];
        char self_path[1024];
        if (readlink("/proc/self/exe", self_path, sizeof(self_path) - 1) > 0) {
            snprintf(default_key, sizeof(default_key), "%s/filly-key.sec", dirname(self_path));
        } else {
            snprintf(default_key, sizeof(default_key), "tools/filly-key.sec");
        }
        keyfile = default_key;
    }

    unsigned char *sk_hex = NULL;
    size_t sk_hex_len = 0;
    if (read_file(keyfile, &sk_hex, &sk_hex_len) < 0) {
        fprintf(stderr, "Cannot read secret key from %s\n", keyfile);
        fprintf(stderr, "Run 'genkey' first or set FILLY_SIGN_KEY\n");
        return 1;
    }
    if (sk_hex_len > 0 && sk_hex[sk_hex_len - 1] == '\n') sk_hex[--sk_hex_len] = '\0';
    if (sk_hex_len != crypto_sign_SECRETKEYBYTES * 2) {
        fprintf(stderr, "Invalid secret key length: expected %d hex chars, got %zu\n",
                crypto_sign_SECRETKEYBYTES * 2, sk_hex_len);
        free(sk_hex);
        return 1;
    }
    unsigned char sk[crypto_sign_SECRETKEYBYTES];
    if (!hex_to_bytes((char *)sk_hex, sk, crypto_sign_SECRETKEYBYTES)) {
        fprintf(stderr, "Invalid hex in secret key\n");
        free(sk_hex);
        return 1;
    }
    free(sk_hex);

    unsigned char *plugin_data = NULL;
    size_t plugin_size = 0;
    if (read_file(argv[1], &plugin_data, &plugin_size) < 0) {
        perror("Cannot read plugin file");
        return 1;
    }

    unsigned char sig[crypto_sign_BYTES];
    crypto_sign_detached(sig, NULL, plugin_data, plugin_size, sk);

    char out_path[2056];
    if (argc >= 3) {
        snprintf(out_path, sizeof(out_path), "%s", argv[2]);
    } else {
        snprintf(out_path, sizeof(out_path), "%s.sig", argv[1]);
    }

    FILE *out = fopen(out_path, "wb");
    if (!out) { perror("fopen output"); free(plugin_data); return 1; }
    fwrite(sig, 1, crypto_sign_BYTES, out);
    fclose(out);
    free(plugin_data);

    printf("Signed: %s -> %s (%d bytes)\n", argv[1], out_path, crypto_sign_BYTES);
    return 0;
}