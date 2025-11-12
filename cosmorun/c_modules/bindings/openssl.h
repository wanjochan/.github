/* OpenSSL FFI Bindings for CosmoRun
 * Provides common OpenSSL functions for SSL/TLS, crypto, and hashing
 *
 * Usage: __import("bindings/openssl")
 */

#ifndef COSMORUN_BINDINGS_OPENSSL_H
#define COSMORUN_BINDINGS_OPENSSL_H

/* TCC compatibility: include types before standard headers */
#ifdef __TINYC__
#include "tcc_stdint.h"
#endif

#include <stddef.h>
#ifndef __TINYC__
#include <stdint.h>
#endif

/* SSL/TLS Types */
typedef struct ssl_method_st SSL_METHOD;
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_st SSL;
typedef struct bio_st BIO;
typedef struct x509_st X509;
typedef struct evp_pkey_st EVP_PKEY;
typedef struct evp_md_ctx_st EVP_MD_CTX;
typedef struct evp_cipher_ctx_st EVP_CIPHER_CTX;
typedef struct evp_md_st EVP_MD;
typedef struct evp_cipher_st EVP_CIPHER;

/* SSL Constants */
#define SSL_FILETYPE_PEM        1
#define SSL_FILETYPE_ASN1       2

#define SSL_ERROR_NONE              0
#define SSL_ERROR_SSL               1
#define SSL_ERROR_WANT_READ         2
#define SSL_ERROR_WANT_WRITE        3
#define SSL_ERROR_SYSCALL           5
#define SSL_ERROR_ZERO_RETURN       6

/* SSL/TLS Functions */
const SSL_METHOD* TLS_client_method(void);
const SSL_METHOD* TLS_server_method(void);
const SSL_METHOD* TLS_method(void);

SSL_CTX* SSL_CTX_new(const SSL_METHOD* method);
void SSL_CTX_free(SSL_CTX* ctx);
int SSL_CTX_use_certificate_file(SSL_CTX* ctx, const char* file, int type);
int SSL_CTX_use_PrivateKey_file(SSL_CTX* ctx, const char* file, int type);
int SSL_CTX_check_private_key(const SSL_CTX* ctx);
int SSL_CTX_load_verify_locations(SSL_CTX* ctx, const char* CAfile, const char* CApath);
void SSL_CTX_set_verify(SSL_CTX* ctx, int mode, void* callback);

SSL* SSL_new(SSL_CTX* ctx);
void SSL_free(SSL* ssl);
int SSL_set_fd(SSL* ssl, int fd);
int SSL_connect(SSL* ssl);
int SSL_accept(SSL* ssl);
int SSL_read(SSL* ssl, void* buf, int num);
int SSL_write(SSL* ssl, const void* buf, int num);
int SSL_shutdown(SSL* ssl);
int SSL_get_error(const SSL* ssl, int ret);

/* BIO Functions */
BIO* BIO_new_file(const char* filename, const char* mode);
void BIO_free(BIO* bio);
int BIO_read(BIO* bio, void* data, int len);
int BIO_write(BIO* bio, const void* data, int len);

/* Hashing Functions - EVP Interface */
EVP_MD_CTX* EVP_MD_CTX_new(void);
void EVP_MD_CTX_free(EVP_MD_CTX* ctx);
int EVP_DigestInit_ex(EVP_MD_CTX* ctx, const EVP_MD* type, void* impl);
int EVP_DigestUpdate(EVP_MD_CTX* ctx, const void* d, size_t cnt);
int EVP_DigestFinal_ex(EVP_MD_CTX* ctx, unsigned char* md, unsigned int* s);

const EVP_MD* EVP_md5(void);
const EVP_MD* EVP_sha1(void);
const EVP_MD* EVP_sha256(void);
const EVP_MD* EVP_sha512(void);

/* Encryption Functions - EVP Interface */
EVP_CIPHER_CTX* EVP_CIPHER_CTX_new(void);
void EVP_CIPHER_CTX_free(EVP_CIPHER_CTX* ctx);
int EVP_EncryptInit_ex(EVP_CIPHER_CTX* ctx, const EVP_CIPHER* cipher, void* impl, const unsigned char* key, const unsigned char* iv);
int EVP_EncryptUpdate(EVP_CIPHER_CTX* ctx, unsigned char* out, int* outl, const unsigned char* in, int inl);
int EVP_EncryptFinal_ex(EVP_CIPHER_CTX* ctx, unsigned char* out, int* outl);
int EVP_DecryptInit_ex(EVP_CIPHER_CTX* ctx, const EVP_CIPHER* cipher, void* impl, const unsigned char* key, const unsigned char* iv);
int EVP_DecryptUpdate(EVP_CIPHER_CTX* ctx, unsigned char* out, int* outl, const unsigned char* in, int inl);
int EVP_DecryptFinal_ex(EVP_CIPHER_CTX* ctx, unsigned char* out, int* outl);

const EVP_CIPHER* EVP_aes_128_cbc(void);
const EVP_CIPHER* EVP_aes_256_cbc(void);
const EVP_CIPHER* EVP_aes_128_gcm(void);
const EVP_CIPHER* EVP_aes_256_gcm(void);

/* Random number generation */
int RAND_bytes(unsigned char* buf, int num);
int RAND_pseudo_bytes(unsigned char* buf, int num);

/* Error handling */
unsigned long ERR_get_error(void);
void ERR_error_string_n(unsigned long e, char* buf, size_t len);
void ERR_print_errors_fp(void* fp);

/* Library initialization */
void OPENSSL_init_ssl(uint64_t opts, void* settings);
void OPENSSL_init_crypto(uint64_t opts, void* settings);

/* High-level wrapper functions for convenience */

/* SSL Context wrapper */
typedef struct {
    SSL_CTX* ctx;
    SSL* ssl;
    int fd;
    int is_server;
} ssl_context_t;

/* Create SSL context for client */
static inline ssl_context_t* ssl_context_client_new(void) {
    ssl_context_t* sctx = (ssl_context_t*)malloc(sizeof(ssl_context_t));
    if (!sctx) return NULL;

    sctx->ctx = SSL_CTX_new(TLS_client_method());
    sctx->ssl = NULL;
    sctx->fd = -1;
    sctx->is_server = 0;
    return sctx;
}

/* Create SSL context for server */
static inline ssl_context_t* ssl_context_server_new(const char* cert_file, const char* key_file) {
    ssl_context_t* sctx = (ssl_context_t*)malloc(sizeof(ssl_context_t));
    if (!sctx) return NULL;

    sctx->ctx = SSL_CTX_new(TLS_server_method());
    if (!sctx->ctx) {
        free(sctx);
        return NULL;
    }

    if (SSL_CTX_use_certificate_file(sctx->ctx, cert_file, SSL_FILETYPE_PEM) <= 0) {
        SSL_CTX_free(sctx->ctx);
        free(sctx);
        return NULL;
    }

    if (SSL_CTX_use_PrivateKey_file(sctx->ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
        SSL_CTX_free(sctx->ctx);
        free(sctx);
        return NULL;
    }

    if (!SSL_CTX_check_private_key(sctx->ctx)) {
        SSL_CTX_free(sctx->ctx);
        free(sctx);
        return NULL;
    }

    sctx->ssl = NULL;
    sctx->fd = -1;
    sctx->is_server = 1;
    return sctx;
}

/* Connect to server (client) */
static inline int ssl_context_connect(ssl_context_t* sctx, int fd) {
    if (!sctx || sctx->is_server) return -1;

    sctx->ssl = SSL_new(sctx->ctx);
    if (!sctx->ssl) return -1;

    if (SSL_set_fd(sctx->ssl, fd) != 1) {
        SSL_free(sctx->ssl);
        sctx->ssl = NULL;
        return -1;
    }

    sctx->fd = fd;
    return SSL_connect(sctx->ssl);
}

/* Accept connection (server) */
static inline int ssl_context_accept(ssl_context_t* sctx, int fd) {
    if (!sctx || !sctx->is_server) return -1;

    sctx->ssl = SSL_new(sctx->ctx);
    if (!sctx->ssl) return -1;

    if (SSL_set_fd(sctx->ssl, fd) != 1) {
        SSL_free(sctx->ssl);
        sctx->ssl = NULL;
        return -1;
    }

    sctx->fd = fd;
    return SSL_accept(sctx->ssl);
}

/* Read from SSL connection */
static inline int ssl_context_read(ssl_context_t* sctx, void* buf, int len) {
    if (!sctx || !sctx->ssl) return -1;
    return SSL_read(sctx->ssl, buf, len);
}

/* Write to SSL connection */
static inline int ssl_context_write(ssl_context_t* sctx, const void* buf, int len) {
    if (!sctx || !sctx->ssl) return -1;
    return SSL_write(sctx->ssl, buf, len);
}

/* Free SSL context */
static inline void ssl_context_free(ssl_context_t* sctx) {
    if (!sctx) return;
    if (sctx->ssl) {
        SSL_shutdown(sctx->ssl);
        SSL_free(sctx->ssl);
    }
    if (sctx->ctx) SSL_CTX_free(sctx->ctx);
    free(sctx);
}

/* Simple hash function wrapper */
static inline int hash_data(const EVP_MD* md, const void* data, size_t len, unsigned char* hash_out, unsigned int* hash_len) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (!ctx) return 0;

    int ret = 0;
    if (EVP_DigestInit_ex(ctx, md, NULL) &&
        EVP_DigestUpdate(ctx, data, len) &&
        EVP_DigestFinal_ex(ctx, hash_out, hash_len)) {
        ret = 1;
    }

    EVP_MD_CTX_free(ctx);
    return ret;
}

/* Convenience hash functions */
static inline int sha256_hash(const void* data, size_t len, unsigned char hash_out[32]) {
    unsigned int hash_len;
    return hash_data(EVP_sha256(), data, len, hash_out, &hash_len);
}

static inline int sha512_hash(const void* data, size_t len, unsigned char hash_out[64]) {
    unsigned int hash_len;
    return hash_data(EVP_sha512(), data, len, hash_out, &hash_len);
}

static inline int md5_hash(const void* data, size_t len, unsigned char hash_out[16]) {
    unsigned int hash_len;
    return hash_data(EVP_md5(), data, len, hash_out, &hash_len);
}

#endif /* COSMORUN_BINDINGS_OPENSSL_H */
