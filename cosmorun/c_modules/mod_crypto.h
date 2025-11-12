#ifndef COSMORUN_CRYPTO_H
#define COSMORUN_CRYPTO_H

/*
 * mod_crypto - Node.js-style cryptographic module for cosmorun
 *
 * Provides cryptographic operations similar to Node.js crypto:
 * - Hash functions: MD5, SHA1, SHA256, SHA512
 * - HMAC (Hash-based Message Authentication Code)
 * - AES encryption/decryption (CBC mode)
 * - Secure random number generation
 * - Hex encoding utilities
 */

#include "src/cosmo_libc.h"

/* ==================== Hash Types ==================== */

typedef enum {
    CRYPTO_HASH_MD5 = 0,
    CRYPTO_HASH_SHA1,
    CRYPTO_HASH_SHA256,
    CRYPTO_HASH_SHA512
} crypto_hash_type_t;

/* Hash digest sizes */
#define CRYPTO_MD5_DIGEST_SIZE    16
#define CRYPTO_SHA1_DIGEST_SIZE   20
#define CRYPTO_SHA256_DIGEST_SIZE 32
#define CRYPTO_SHA512_DIGEST_SIZE 64

/* Maximum digest size */
#define CRYPTO_MAX_DIGEST_SIZE 64

/* ==================== Cipher Types ==================== */

typedef enum {
    CRYPTO_CIPHER_AES_128_CBC = 0,
    CRYPTO_CIPHER_AES_192_CBC,
    CRYPTO_CIPHER_AES_256_CBC,
    CRYPTO_CIPHER_AES_128_ECB,
    CRYPTO_CIPHER_AES_192_ECB,
    CRYPTO_CIPHER_AES_256_ECB
} crypto_cipher_type_t;

/* AES block size (always 16 bytes) */
#define CRYPTO_AES_BLOCK_SIZE 16
#define CRYPTO_AES_IV_SIZE    16

/* AES key sizes */
#define CRYPTO_AES_128_KEY_SIZE 16
#define CRYPTO_AES_192_KEY_SIZE 24
#define CRYPTO_AES_256_KEY_SIZE 32

/* ==================== Hash Context Structures ==================== */

/* MD5 context */
typedef struct {
    uint32_t state[4];
    uint64_t count;
    uint8_t buffer[64];
} crypto_md5_ctx_t;

/* SHA1 context */
typedef struct {
    uint32_t state[5];
    uint64_t count;
    uint8_t buffer[64];
} crypto_sha1_ctx_t;

/* SHA256 context */
typedef struct {
    uint32_t state[8];
    uint64_t count;
    uint8_t buffer[64];
} crypto_sha256_ctx_t;

/* SHA512 context */
typedef struct {
    uint64_t state[8];
    uint64_t count[2];
    uint8_t buffer[128];
} crypto_sha512_ctx_t;

/* Generic hash context */
typedef struct {
    crypto_hash_type_t type;
    union {
        crypto_md5_ctx_t md5;
        crypto_sha1_ctx_t sha1;
        crypto_sha256_ctx_t sha256;
        crypto_sha512_ctx_t sha512;
    } ctx;
} crypto_hash_t;

/* ==================== HMAC Context ==================== */

typedef struct {
    crypto_hash_t hash;
    crypto_hash_type_t hash_type;
    uint8_t key_pad[128];
    size_t block_size;
} crypto_hmac_t;

/* ==================== Cipher Context ==================== */

typedef struct {
    crypto_cipher_type_t type;
    uint8_t key[32];
    uint8_t iv[CRYPTO_AES_IV_SIZE];
    size_t key_size;
    int encrypt_mode;
    /* AES round keys (expanded key schedule) */
    uint32_t round_keys[60];
    int num_rounds;
} crypto_cipher_t;

/* ==================== Hash Functions ==================== */

/* Create hash context */
crypto_hash_t* crypto_hash_create(crypto_hash_type_t type);

/* Update hash with data */
int crypto_hash_update(crypto_hash_t *hash, const uint8_t *data, size_t len);

/* Finalize hash and get digest */
int crypto_hash_final(crypto_hash_t *hash, uint8_t *output);

/* Free hash context */
void crypto_hash_free(crypto_hash_t *hash);

/* One-shot hash calculation */
int crypto_hash_simple(crypto_hash_type_t type, const uint8_t *data,
                       size_t len, uint8_t *output);

/* Get digest size for hash type */
size_t crypto_hash_digest_size(crypto_hash_type_t type);

/* Get hash type name */
const char* crypto_hash_type_name(crypto_hash_type_t type);

/* ==================== HMAC Functions ==================== */

/* Create HMAC context */
crypto_hmac_t* crypto_hmac_create(crypto_hash_type_t type,
                                   const uint8_t *key, size_t key_len);

/* Update HMAC with data */
int crypto_hmac_update(crypto_hmac_t *hmac, const uint8_t *data, size_t len);

/* Finalize HMAC and get digest */
int crypto_hmac_final(crypto_hmac_t *hmac, uint8_t *output);

/* Free HMAC context */
void crypto_hmac_free(crypto_hmac_t *hmac);

/* One-shot HMAC calculation */
int crypto_hmac_simple(crypto_hash_type_t type, const uint8_t *key,
                       size_t key_len, const uint8_t *data,
                       size_t data_len, uint8_t *output);

/* ==================== Cipher Functions ==================== */

/* Create cipher context */
crypto_cipher_t* crypto_cipher_create(crypto_cipher_type_t type,
                                       const uint8_t *key,
                                       const uint8_t *iv);

/* Encrypt data (output must have space for padding) */
int crypto_cipher_encrypt(crypto_cipher_t *cipher, const uint8_t *input,
                          size_t input_len, uint8_t *output, size_t *output_len);

/* Decrypt data (removes padding) */
int crypto_cipher_decrypt(crypto_cipher_t *cipher, const uint8_t *input,
                          size_t input_len, uint8_t *output, size_t *output_len);

/* Free cipher context */
void crypto_cipher_free(crypto_cipher_t *cipher);

/* ==================== Utility Functions ==================== */

/* Generate cryptographically secure random bytes */
int crypto_random_bytes(uint8_t *buffer, size_t len);

/* Convert binary data to hex string (output must be 2*len + 1 bytes) */
char* crypto_hex_encode(const uint8_t *data, size_t len);

/* Convert hex string to binary data */
int crypto_hex_decode(const char *hex, uint8_t *output, size_t *output_len);

/* Apply PKCS7 padding */
int crypto_pkcs7_pad(const uint8_t *input, size_t input_len,
                     uint8_t *output, size_t *output_len, size_t block_size);

/* Remove PKCS7 padding */
int crypto_pkcs7_unpad(const uint8_t *input, size_t input_len,
                       uint8_t *output, size_t *output_len);

/* Zero out sensitive memory */
void crypto_secure_zero(void *ptr, size_t len);

#endif /* COSMORUN_CRYPTO_H */
