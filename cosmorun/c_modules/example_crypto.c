/*
 * example_crypto.c - Examples for mod_crypto usage
 *
 * Demonstrates:
 * - Hash functions (MD5, SHA256, SHA512)
 * - HMAC authentication
 * - Secure random number generation
 * - Hex encoding/decoding
 * - Best practices for cryptographic operations
 */

#include "src/cosmo_libc.h"
#include "mod_crypto.c"

void example_hash_functions() {
    printf("\n========================================\n");
    printf("  Hash Functions\n");
    printf("========================================\n\n");

    /* Example 1: Simple SHA256 hash */
    printf("1. SHA256 Hash:\n");
    const char *data = "Hello, World!";
    uint8_t digest[CRYPTO_SHA256_DIGEST_SIZE];

    crypto_hash_simple(CRYPTO_HASH_SHA256, (const uint8_t*)data, strlen(data), digest);

    char *hex = crypto_hex_encode(digest, CRYPTO_SHA256_DIGEST_SIZE);
    printf("   Input:  %s\n", data);
    printf("   SHA256: %s\n\n", hex);
    free(hex);

    /* Example 2: Incremental hashing (for large files) */
    printf("2. Incremental Hashing:\n");
    crypto_hash_t *hash = crypto_hash_create(CRYPTO_HASH_SHA256);

    crypto_hash_update(hash, (const uint8_t*)"Part 1: ", 8);
    crypto_hash_update(hash, (const uint8_t*)"Part 2: ", 8);
    crypto_hash_update(hash, (const uint8_t*)"Part 3", 6);

    crypto_hash_final(hash, digest);
    crypto_hash_free(hash);

    hex = crypto_hex_encode(digest, CRYPTO_SHA256_DIGEST_SIZE);
    printf("   Combined input: 'Part 1: Part 2: Part 3'\n");
    printf("   SHA256: %s\n\n", hex);
    free(hex);

    /* Example 3: Different hash algorithms */
    printf("3. Different Hash Algorithms:\n");
    const char *message = "Test message";

    uint8_t md5_digest[CRYPTO_MD5_DIGEST_SIZE];
    crypto_hash_simple(CRYPTO_HASH_MD5, (const uint8_t*)message, strlen(message), md5_digest);
    hex = crypto_hex_encode(md5_digest, CRYPTO_MD5_DIGEST_SIZE);
    printf("   MD5:    %s\n", hex);
    free(hex);

    uint8_t sha256_digest[CRYPTO_SHA256_DIGEST_SIZE];
    crypto_hash_simple(CRYPTO_HASH_SHA256, (const uint8_t*)message, strlen(message), sha256_digest);
    hex = crypto_hex_encode(sha256_digest, CRYPTO_SHA256_DIGEST_SIZE);
    printf("   SHA256: %s\n", hex);
    free(hex);

    uint8_t sha512_digest[CRYPTO_SHA512_DIGEST_SIZE];
    crypto_hash_simple(CRYPTO_HASH_SHA512, (const uint8_t*)message, strlen(message), sha512_digest);
    hex = crypto_hex_encode(sha512_digest, CRYPTO_SHA512_DIGEST_SIZE);
    printf("   SHA512: %s\n", hex);
    free(hex);
}

void example_hmac() {
    printf("\n========================================\n");
    printf("  HMAC (Message Authentication)\n");
    printf("========================================\n\n");

    /* Example 1: Simple HMAC-SHA256 */
    printf("1. HMAC-SHA256 Authentication:\n");
    const char *key = "my-secret-key";
    const char *message = "Important message";
    uint8_t mac[CRYPTO_SHA256_DIGEST_SIZE];

    crypto_hmac_simple(CRYPTO_HASH_SHA256,
                       (const uint8_t*)key, strlen(key),
                       (const uint8_t*)message, strlen(message),
                       mac);

    char *hex = crypto_hex_encode(mac, CRYPTO_SHA256_DIGEST_SIZE);
    printf("   Key:     %s\n", key);
    printf("   Message: %s\n", message);
    printf("   HMAC:    %s\n\n", hex);
    free(hex);

    /* Example 2: Verifying HMAC */
    printf("2. HMAC Verification:\n");
    uint8_t mac_verify[CRYPTO_SHA256_DIGEST_SIZE];

    crypto_hmac_simple(CRYPTO_HASH_SHA256,
                       (const uint8_t*)key, strlen(key),
                       (const uint8_t*)message, strlen(message),
                       mac_verify);

    int valid = (memcmp(mac, mac_verify, CRYPTO_SHA256_DIGEST_SIZE) == 0);
    printf("   Message verified: %s\n\n", valid ? "YES" : "NO");

    /* Example 3: HMAC with SHA512 for stronger security */
    printf("3. HMAC-SHA512 (stronger):\n");
    uint8_t mac512[CRYPTO_SHA512_DIGEST_SIZE];

    crypto_hmac_simple(CRYPTO_HASH_SHA512,
                       (const uint8_t*)key, strlen(key),
                       (const uint8_t*)message, strlen(message),
                       mac512);

    hex = crypto_hex_encode(mac512, CRYPTO_SHA512_DIGEST_SIZE);
    printf("   HMAC-SHA512: %s\n", hex);
    free(hex);
}

void example_random() {
    printf("\n========================================\n");
    printf("  Secure Random Number Generation\n");
    printf("========================================\n\n");

    /* Example 1: Generate random bytes */
    printf("1. Random Bytes:\n");
    uint8_t random_bytes[16];
    crypto_random_bytes(random_bytes, sizeof(random_bytes));

    char *hex = crypto_hex_encode(random_bytes, sizeof(random_bytes));
    printf("   Random (hex): %s\n\n", hex);
    free(hex);

    /* Example 2: Generate random token */
    printf("2. Random Token (32 bytes):\n");
    uint8_t token[32];
    crypto_random_bytes(token, sizeof(token));

    hex = crypto_hex_encode(token, sizeof(token));
    printf("   Token: %s\n\n", hex);
    free(hex);

    /* Example 3: Generate random IV for encryption */
    printf("3. Random IV for AES:\n");
    uint8_t iv[CRYPTO_AES_IV_SIZE];
    crypto_random_bytes(iv, sizeof(iv));

    hex = crypto_hex_encode(iv, sizeof(iv));
    printf("   IV (16 bytes): %s\n", hex);
    free(hex);
}

void example_hex_encoding() {
    printf("\n========================================\n");
    printf("  Hex Encoding/Decoding\n");
    printf("========================================\n\n");

    /* Example 1: Encode binary to hex */
    printf("1. Binary to Hex:\n");
    const uint8_t binary[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE};
    char *hex = crypto_hex_encode(binary, sizeof(binary));
    printf("   Binary: [0xDE, 0xAD, 0xBE, 0xEF, 0xCA, 0xFE, 0xBA, 0xBE]\n");
    printf("   Hex:    %s\n\n", hex);
    free(hex);

    /* Example 2: Decode hex to binary */
    printf("2. Hex to Binary:\n");
    const char *hex_string = "48656c6c6f";
    uint8_t decoded[16];
    size_t decoded_len;

    crypto_hex_decode(hex_string, decoded, &decoded_len);
    printf("   Hex:    %s\n", hex_string);
    printf("   Binary: [");
    for (size_t i = 0; i < decoded_len; i++) {
        printf("%s0x%02X", i > 0 ? ", " : "", decoded[i]);
    }
    printf("]\n");
    printf("   ASCII:  %.*s\n", (int)decoded_len, decoded);
}

void example_password_hashing() {
    printf("\n========================================\n");
    printf("  Password Hashing Best Practices\n");
    printf("========================================\n\n");

    printf("IMPORTANT: For production password hashing, use:\n");
    printf("  - bcrypt, scrypt, or Argon2 (not available in this module)\n");
    printf("  - These algorithms are designed to be slow (resist brute force)\n");
    printf("  - SHA256/SHA512 are TOO FAST for passwords!\n\n");

    printf("Educational example (NOT for production):\n");

    const char *password = "my-password";
    const char *salt = "random-salt-12345678";

    /* Combine password + salt */
    char salted[256];
    snprintf(salted, sizeof(salted), "%s%s", password, salt);

    uint8_t hash[CRYPTO_SHA256_DIGEST_SIZE];
    crypto_hash_simple(CRYPTO_HASH_SHA256, (const uint8_t*)salted, strlen(salted), hash);

    char *hex = crypto_hex_encode(hash, CRYPTO_SHA256_DIGEST_SIZE);
    printf("   Password: %s\n", password);
    printf("   Salt:     %s\n", salt);
    printf("   Hash:     %s\n", hex);
    printf("\n   WARNING: Use proper password hashing in production!\n");
    free(hex);
}

void example_security_tips() {
    printf("\n========================================\n");
    printf("  Security Best Practices\n");
    printf("========================================\n\n");

    printf("1. Hash Function Selection:\n");
    printf("   - SHA256/SHA512: Good for integrity checks\n");
    printf("   - MD5: BROKEN, only for non-security uses\n");
    printf("   - SHA1: WEAK, avoid for new applications\n\n");

    printf("2. HMAC for Authentication:\n");
    printf("   - Always use HMAC for message authentication\n");
    printf("   - Never use plain hash for authentication\n");
    printf("   - Use SHA256 or SHA512 as underlying hash\n\n");

    printf("3. Random Number Generation:\n");
    printf("   - Always use crypto_random_bytes() for keys/IVs\n");
    printf("   - Never use rand() for security purposes\n");
    printf("   - Ensure /dev/urandom is available\n\n");

    printf("4. Key Management:\n");
    printf("   - Store keys securely (not in code)\n");
    printf("   - Use proper key derivation (PBKDF2, scrypt)\n");
    printf("   - Zero out keys after use (crypto_secure_zero)\n\n");

    printf("5. Data Integrity:\n");
    printf("   - Hash files before and after transmission\n");
    printf("   - Use HMAC for authenticated messages\n");
    printf("   - Verify all signatures before trusting data\n");

    /* Demonstrate secure key zeroing */
    printf("\n6. Example: Secure Key Handling:\n");
    uint8_t secret_key[32];
    crypto_random_bytes(secret_key, sizeof(secret_key));

    char *hex = crypto_hex_encode(secret_key, sizeof(secret_key));
    printf("   Generated key: %s\n", hex);
    free(hex);

    /* Use the key... */

    /* Zero it out when done */
    crypto_secure_zero(secret_key, sizeof(secret_key));
    printf("   Key zeroed out (secure cleanup)\n");
}

void example_file_integrity() {
    printf("\n========================================\n");
    printf("  File Integrity Checking\n");
    printf("========================================\n\n");

    printf("Typical use case: Verify file hasn't been tampered with\n\n");

    /* Simulate file content */
    const char *file_content = "This is the content of my important file.\n"
                               "It contains sensitive information.\n"
                               "We want to ensure it hasn't been modified.\n";

    /* Calculate hash */
    uint8_t file_hash[CRYPTO_SHA256_DIGEST_SIZE];
    crypto_hash_simple(CRYPTO_HASH_SHA256,
                       (const uint8_t*)file_content, strlen(file_content),
                       file_hash);

    char *hex = crypto_hex_encode(file_hash, CRYPTO_SHA256_DIGEST_SIZE);
    printf("Original file SHA256: %s\n\n", hex);

    /* Later, verify the file */
    uint8_t verify_hash[CRYPTO_SHA256_DIGEST_SIZE];
    crypto_hash_simple(CRYPTO_HASH_SHA256,
                       (const uint8_t*)file_content, strlen(file_content),
                       verify_hash);

    int intact = (memcmp(file_hash, verify_hash, CRYPTO_SHA256_DIGEST_SIZE) == 0);
    printf("File integrity check: %s\n", intact ? "PASSED" : "FAILED");

    free(hex);
}

int main() {
    printf("\n╔══════════════════════════════════════╗\n");
    printf("║   mod_crypto Examples & Tutorials    ║\n");
    printf("╚══════════════════════════════════════╝\n");

    example_hash_functions();
    example_hmac();
    example_random();
    example_hex_encoding();
    example_password_hashing();
    example_file_integrity();
    example_security_tips();

    printf("\n========================================\n");
    printf("  For more information, see:\n");
    printf("  - mod_crypto.h (API reference)\n");
    printf("  - test_mod_crypto.c (test vectors)\n");
    printf("========================================\n\n");

    return 0;
}
