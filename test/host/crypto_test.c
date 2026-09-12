#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "uECC.h"
#include "sha256.h"

static void print_hex(const char *label, const uint8_t *buf, size_t len) {
    printf("%s: ", label);
    for (size_t i = 0; i < len; i++) printf("%02x", buf[i]);
    printf("\n");
}

static void sha256_hash(const uint8_t *data, size_t len, uint8_t out[32]) {
    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, out);
}

static int run_case(const char *name, int passed, int expect_pass) {
    int ok = (passed == expect_pass);
    printf("[%s] %-45s -> %s (%s)\n",
           ok ? " OK " : "FAIL",
           name,
           passed ? "VERIFY PASSED" : "VERIFY FAILED",
           expect_pass ? "expected: PASS" : "expected: FAIL");
    return ok;
}

int main(void) {
    printf("=== Nova-Secure Phase 1: Crypto Foundation Test ===\n\n");

    const struct uECC_Curve_t *curve = uECC_secp256r1();

    uint8_t public_key[64];
    uint8_t private_key[32];

    printf("[*] Generating ECDSA (secp256r1) keypair...\n");
    if (!uECC_make_key(public_key, private_key, curve)) {
        fprintf(stderr, "uECC_make_key() failed\n");
        return 1;
    }
    print_hex("    public_key ", public_key, 64);
    print_hex("    private_key", private_key, 32);
    printf("\n");

    /* Simulated "firmware" blob */
    const char *firmware_data = "NOVA-SECURE-FIRMWARE-IMAGE-v1.0-PAYLOAD-BYTES";
    uint8_t hash[32];
    sha256_hash((const uint8_t *)firmware_data, strlen(firmware_data), hash);
    print_hex("[*] SHA-256(firmware)", hash, 32);

    uint8_t signature[64];
    printf("[*] Signing hash with private key...\n");
    if (!uECC_sign(private_key, hash, sizeof(hash), signature, curve)) {
        fprintf(stderr, "uECC_sign() failed\n");
        return 1;
    }
    print_hex("    signature", signature, 64);
    printf("\n");

    int all_ok = 1;

    /* Case 1: valid firmware, valid signature -> should PASS */
    {
        int ok = uECC_verify(public_key, hash, sizeof(hash), signature, curve);
        all_ok &= run_case("Valid firmware + valid signature", ok, 1);
    }

    {
        char tampered[64];
        strcpy(tampered, firmware_data);
        tampered[10] ^= 0x01; /* flip one bit */
        uint8_t tampered_hash[32];
        sha256_hash((const uint8_t *)tampered, strlen(tampered), tampered_hash);
        int ok = uECC_verify(public_key, tampered_hash, sizeof(tampered_hash), signature, curve);
        all_ok &= run_case("Tampered firmware (1 bit flip)", ok, 0);
    }

    /* Case 3: tamper with the signature itself -> should FAIL */
    {
        uint8_t bad_sig[64];
        memcpy(bad_sig, signature, 64);
        bad_sig[0] ^= 0xFF;
        int ok = uECC_verify(public_key, hash, sizeof(hash), bad_sig, curve);
        all_ok &= run_case("Tampered signature (1 byte)", ok, 0);
    }

    /* Case 4: verify against a DIFFERENT (wrong) public key -> should FAIL */
    {
        uint8_t other_pub[64], other_priv[32];
        uECC_make_key(other_pub, other_priv, curve);
        int ok = uECC_verify(other_pub, hash, sizeof(hash), signature, curve);
        all_ok &= run_case("Wrong public key", ok, 0);
    }

    printf("\n=== %s ===\n", all_ok ? "ALL TESTS PASSED" : "SOME TESTS FAILED");
    return all_ok ? 0 : 1;
}