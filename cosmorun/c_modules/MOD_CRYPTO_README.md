# mod_crypto - Cryptographic functions implementation

**File**: `c_modules/mod_crypto.c`
**Dependencies**:

- mod_crypto (internal)
- mod_error_impl (internal)

## Overview

`mod_crypto` provides cryptographic functions implementation.

## Quick Start

```c
#include "mod_crypto.c"

// TODO: Add minimal example
```

## API Reference

#### `void crypto_secure_zero(void *ptr, size_t len)`
- **Parameters**:
  - `void *ptr`
  - `size_t len`
- **Returns**: None

#### `size_t crypto_hash_digest_size(crypto_hash_type_t type)`
- **Parameters**:
  - `crypto_hash_type_t type`
- **Returns**: `size_t`

#### `crypto_hash_t* crypto_hash_create(crypto_hash_type_t type)`
- **Parameters**:
  - `crypto_hash_type_t type`
- **Returns**: `crypto_hash_t*`

#### `int crypto_hash_update(crypto_hash_t *hash, const uint8_t *data, size_t len)`
- **Parameters**:
  - `crypto_hash_t *hash`
  - `const uint8_t *data`
  - `size_t len`
- **Returns**: `int`

#### `int crypto_hash_final(crypto_hash_t *hash, uint8_t *output)`
- **Parameters**:
  - `crypto_hash_t *hash`
  - `uint8_t *output`
- **Returns**: `int`

#### `void crypto_hash_free(crypto_hash_t *hash)`
- **Parameters**:
  - `crypto_hash_t *hash`
- **Returns**: None

#### `int crypto_hash_simple(crypto_hash_type_t type, const uint8_t *data, size_t len, uint8_t *output)`
- **Parameters**:
  - `crypto_hash_type_t type`
  - `const uint8_t *data`
  - `size_t len`
  - `uint8_t *output`
- **Returns**: `int`

#### `crypto_hmac_t* crypto_hmac_create(crypto_hash_type_t type, const uint8_t *key, size_t key_len)`
- **Parameters**:
  - `crypto_hash_type_t type`
  - `const uint8_t *key`
  - `size_t key_len`
- **Returns**: `crypto_hmac_t*`

#### `int crypto_hmac_update(crypto_hmac_t *hmac, const uint8_t *data, size_t len)`
- **Parameters**:
  - `crypto_hmac_t *hmac`
  - `const uint8_t *data`
  - `size_t len`
- **Returns**: `int`

#### `int crypto_hmac_final(crypto_hmac_t *hmac, uint8_t *output)`
- **Parameters**:
  - `crypto_hmac_t *hmac`
  - `uint8_t *output`
- **Returns**: `int`

#### `void crypto_hmac_free(crypto_hmac_t *hmac)`
- **Parameters**:
  - `crypto_hmac_t *hmac`
- **Returns**: None

#### `int crypto_hmac_simple(crypto_hash_type_t type, const uint8_t *key, size_t key_len, const uint8_t *data, size_t data_len, uint8_t *output)`
- **Parameters**:
  - `crypto_hash_type_t type`
  - `const uint8_t *key`
  - `size_t key_len`
  - `const uint8_t *data`
  - `size_t data_len`
  - `uint8_t *output`
- **Returns**: `int`

#### `int crypto_random_bytes(uint8_t *buffer, size_t len)`
- **Parameters**:
  - `uint8_t *buffer`
  - `size_t len`
- **Returns**: `int`

#### `char* crypto_hex_encode(const uint8_t *data, size_t len)`
- **Parameters**:
  - `const uint8_t *data`
  - `size_t len`
- **Returns**: `char*`

#### `int crypto_hex_decode(const char *hex, uint8_t *output, size_t *output_len)`
- **Parameters**:
  - `const char *hex`
  - `uint8_t *output`
  - `size_t *output_len`
- **Returns**: `int`

#### `int crypto_pkcs7_pad(const uint8_t *input, size_t input_len, uint8_t *output, size_t *output_len, size_t block_size)`
- **Parameters**:
  - `const uint8_t *input`
  - `size_t input_len`
  - `uint8_t *output`
  - `size_t *output_len`
  - `size_t block_size`
- **Returns**: `int`

#### `int crypto_pkcs7_unpad(const uint8_t *input, size_t input_len, uint8_t *output, size_t *output_len)`
- **Parameters**:
  - `const uint8_t *input`
  - `size_t input_len`
  - `uint8_t *output`
  - `size_t *output_len`
- **Returns**: `int`

#### `crypto_cipher_t* crypto_cipher_create(crypto_cipher_type_t type, const uint8_t *key, const uint8_t *iv)`
- **Parameters**:
  - `crypto_cipher_type_t type`
  - `const uint8_t *key`
  - `const uint8_t *iv`
- **Returns**: `crypto_cipher_t*`

#### `int crypto_cipher_encrypt(crypto_cipher_t *cipher, const uint8_t *input, size_t input_len, uint8_t *output, size_t *output_len)`
- **Description**: AES implementation would go here - this is a stub for now
- **Parameters**:
  - `crypto_cipher_t *cipher`
  - `const uint8_t *input`
  - `size_t input_len`
  - `uint8_t *output`
  - `size_t *output_len`
- **Returns**: `int`

#### `int crypto_cipher_decrypt(crypto_cipher_t *cipher, const uint8_t *input, size_t input_len, uint8_t *output, size_t *output_len)`
- **Description**: AES encryption would go here - this is a stub for now
- **Parameters**:
  - `crypto_cipher_t *cipher`
  - `const uint8_t *input`
  - `size_t input_len`
  - `uint8_t *output`
  - `size_t *output_len`
- **Returns**: `int`

#### `void crypto_cipher_free(crypto_cipher_t *cipher)`
- **Description**: AES decryption would go here - this is a stub for now
- **Parameters**:
  - `crypto_cipher_t *cipher`
- **Returns**: None


## Example Usage

```c
// See c_modules/example_crypto.c for complete examples
```

## Memory Management

Functions returning pointers (e.g., `char*`, struct pointers) allocate memory that must be freed by the caller using the corresponding `_free()` function.

## Error Handling

Functions returning `int` typically return:
- `0` or positive value on success
- `-1` on error
- Check errno for system error details

## Thread Safety

Not thread-safe by default. Use external locking for multi-threaded access.

## See Also

- Example: `c_modules/example_crypto.c` (if available)
- Related modules: Check #include dependencies
