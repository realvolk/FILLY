#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <libgen.h>

static void print_usage(void) {
    fprintf(stderr, "Usage: genkey [-o output_dir]\n");
    fprintf(stderr, "Generates an Ed25519 keypair for FILLY plugin signing.\n");
    fprintf(stderr, "  -o dir   Output directory (default: directory containing genkey binary)\n");
}

int main(int argc, char **argv) {
    char out_dir[1024];
    char self_path[1024];
    if (readlink("/proc/self/exe", self_path, sizeof(self_path) - 1) > 0) {
        snprintf(out_dir, sizeof(out_dir), "%s", dirname(self_path));
    } else {
        snprintf(out_dir, sizeof(out_dir), ".");
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            snprintf(out_dir, sizeof(out_dir), "%s", argv[++i]);
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage();
            return 0;
        }
    }

    mkdir(out_dir, 0700);

    if (sodium_init() < 0) { fprintf(stderr, "sodium_init failed\n"); return 1; }

    unsigned char pk[crypto_sign_PUBLICKEYBYTES];
    unsigned char sk[crypto_sign_SECRETKEYBYTES];
    crypto_sign_keypair(pk, sk);

    char pk_path[1152], sk_path[1152];
    snprintf(pk_path, sizeof(pk_path), "%s/filly-key.pub", out_dir);
    snprintf(sk_path, sizeof(sk_path), "%s/filly-key.sec", out_dir);

    FILE *skf = fopen(sk_path, "w");
    if (!skf) { perror("fopen secret key"); return 1; }
    fchmod(fileno(skf), 0600);
    for (int i = 0; i < crypto_sign_SECRETKEYBYTES; i++)
        fprintf(skf, "%02x", sk[i]);
    fprintf(skf, "\n");
    fclose(skf);

    FILE *pkf = fopen(pk_path, "w");
    if (!pkf) { perror("fopen public key"); return 1; }
    fchmod(fileno(pkf), 0644);
    for (int i = 0; i < crypto_sign_PUBLICKEYBYTES; i++)
        fprintf(pkf, "%02x", pk[i]);
    fprintf(pkf, "\n");
    fclose(pkf);

    printf("Keypair generated:\n");
    printf("  Public:  %s\n", pk_path);
    printf("  Secret:  %s (chmod 600)\n", sk_path);
    printf("\n");
    printf("/* Copy this array into src/backend/daemon/verify.c */\n");
    printf("static const unsigned char public_key[crypto_sign_PUBLICKEYBYTES] = {\n    ");
    for (int i = 0; i < crypto_sign_PUBLICKEYBYTES; i++)
        printf("0x%02x%s", pk[i], i < crypto_sign_PUBLICKEYBYTES - 1 ? ", " : "");
    printf("\n};\n");

    return 0;
}