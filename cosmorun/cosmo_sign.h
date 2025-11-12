#ifndef COSMO_SIGN_H
#define COSMO_SIGN_H

/*
 * cosmo_sign - Code signing module for CosmoRun
 *
 * Provides Ed25519 digital signature generation and verification for binaries
 * Features:
 * - Ed25519 keypair generation
 * - Binary signing with SHA256 + Ed25519
 * - Signature verification
 * - Public key trust registry
 */

#include "cosmo_libc.h"

/* Ed25519 constants */
#define ED25519_PUBLIC_KEY_SIZE  32
#define ED25519_PRIVATE_KEY_SIZE 64
#define ED25519_SIGNATURE_SIZE   64
#define ED25519_SEED_SIZE        32

/* File paths */
#define COSMO_SIGN_KEY_DIR      ".cosmorun/keys"
#define COSMO_SIGN_PRIVATE_KEY  "private.key"
#define COSMO_SIGN_PUBLIC_KEY   "public.key"
#define COSMO_SIGN_TRUST_REGISTRY "trusted_keys.json"

/* Error codes */
#define COSMO_SIGN_OK            0
#define COSMO_SIGN_ERR_INVALID  -1
#define COSMO_SIGN_ERR_IO       -2
#define COSMO_SIGN_ERR_CRYPTO   -3
#define COSMO_SIGN_ERR_NOTFOUND -4
#define COSMO_SIGN_ERR_UNTRUSTED -5

/* Ed25519 keypair structure */
typedef struct {
    uint8_t pubkey[ED25519_PUBLIC_KEY_SIZE];
    uint8_t privkey[ED25519_PRIVATE_KEY_SIZE];
} ed25519_keypair_t;

/* Signature file structure (stored as JSON) */
typedef struct {
    char hash[65];           /* SHA256 hex (64 chars + null) */
    char signature[129];     /* Ed25519 sig base64 (approx 88 chars + null) */
    char pubkey[65];         /* Public key base64 (approx 44 chars + null) */
} cosmo_signature_t;

/* ==================== Core Functions ==================== */

/* Generate Ed25519 keypair and save to ~/.cosmorun/keys/ */
int cosmo_sign_keygen(const char* keydir);

/* Sign a binary file - creates <file>.sig */
int cosmo_sign_file(const char* input_path, const char* privkey_path);

/* Verify binary signature - checks <file>.sig */
int cosmo_verify_file(const char* input_path, const char* pubkey_path);

/* Add public key to trust registry */
int cosmo_trust_key(const char* pubkey_b64);

/* Check if public key is trusted */
int cosmo_is_key_trusted(const uint8_t* pubkey);

/* ==================== Low-level Ed25519 Functions ==================== */

/* Generate Ed25519 keypair from random seed */
void ed25519_create_keypair(uint8_t public_key[32], uint8_t private_key[64],
                            const uint8_t seed[32]);

/* Sign message with private key */
void ed25519_sign(uint8_t signature[64], const uint8_t* message, size_t message_len,
                  const uint8_t public_key[32], const uint8_t private_key[64]);

/* Verify signature */
int ed25519_verify(const uint8_t signature[64], const uint8_t* message,
                   size_t message_len, const uint8_t public_key[32]);

/* ==================== Utility Functions ==================== */

/* Base64 encode (returns malloc'd string) */
char* base64_encode(const uint8_t* data, size_t len);

/* Base64 decode (returns number of bytes decoded) */
int base64_decode(const char* b64, uint8_t* output, size_t max_len);

/* Get home directory path */
const char* get_home_dir(void);

/* Create directory recursively */
int mkdir_p(const char* path);

#endif /* COSMO_SIGN_H */
