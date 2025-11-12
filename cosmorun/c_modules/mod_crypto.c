/*
 * mod_crypto.c - Cryptographic functions implementation
 */

#include "mod_crypto.h"
#include "mod_error_impl.h"

/* ==================== Utility Functions ==================== */

void crypto_secure_zero(void *ptr, size_t len) {
    volatile uint8_t *p = (volatile uint8_t*)ptr;
    while (len--) *p++ = 0;
}

/* ==================== MD5 Implementation ==================== */

#define MD5_F(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define MD5_G(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define MD5_H(x, y, z) ((x) ^ (y) ^ (z))
#define MD5_I(x, y, z) ((y) ^ ((x) | ~(z)))

#define MD5_ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

static void md5_transform(uint32_t state[4], const uint8_t block[64]) {
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t x[16];

    for (int i = 0; i < 16; i++) {
        x[i] = ((uint32_t)block[i*4+0]) | ((uint32_t)block[i*4+1] << 8) |
               ((uint32_t)block[i*4+2] << 16) | ((uint32_t)block[i*4+3] << 24);
    }

    /* Round 1 */
    #define MD5_FF(a, b, c, d, x, s, ac) { \
        (a) += MD5_F((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = MD5_ROTATE_LEFT((a), (s)); \
        (a) += (b); \
    }
    MD5_FF(a, b, c, d, x[ 0],  7, 0xd76aa478); MD5_FF(d, a, b, c, x[ 1], 12, 0xe8c7b756);
    MD5_FF(c, d, a, b, x[ 2], 17, 0x242070db); MD5_FF(b, c, d, a, x[ 3], 22, 0xc1bdceee);
    MD5_FF(a, b, c, d, x[ 4],  7, 0xf57c0faf); MD5_FF(d, a, b, c, x[ 5], 12, 0x4787c62a);
    MD5_FF(c, d, a, b, x[ 6], 17, 0xa8304613); MD5_FF(b, c, d, a, x[ 7], 22, 0xfd469501);
    MD5_FF(a, b, c, d, x[ 8],  7, 0x698098d8); MD5_FF(d, a, b, c, x[ 9], 12, 0x8b44f7af);
    MD5_FF(c, d, a, b, x[10], 17, 0xffff5bb1); MD5_FF(b, c, d, a, x[11], 22, 0x895cd7be);
    MD5_FF(a, b, c, d, x[12],  7, 0x6b901122); MD5_FF(d, a, b, c, x[13], 12, 0xfd987193);
    MD5_FF(c, d, a, b, x[14], 17, 0xa679438e); MD5_FF(b, c, d, a, x[15], 22, 0x49b40821);

    /* Round 2 */
    #define MD5_GG(a, b, c, d, x, s, ac) { \
        (a) += MD5_G((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = MD5_ROTATE_LEFT((a), (s)); \
        (a) += (b); \
    }
    MD5_GG(a, b, c, d, x[ 1],  5, 0xf61e2562); MD5_GG(d, a, b, c, x[ 6],  9, 0xc040b340);
    MD5_GG(c, d, a, b, x[11], 14, 0x265e5a51); MD5_GG(b, c, d, a, x[ 0], 20, 0xe9b6c7aa);
    MD5_GG(a, b, c, d, x[ 5],  5, 0xd62f105d); MD5_GG(d, a, b, c, x[10],  9, 0x02441453);
    MD5_GG(c, d, a, b, x[15], 14, 0xd8a1e681); MD5_GG(b, c, d, a, x[ 4], 20, 0xe7d3fbc8);
    MD5_GG(a, b, c, d, x[ 9],  5, 0x21e1cde6); MD5_GG(d, a, b, c, x[14],  9, 0xc33707d6);
    MD5_GG(c, d, a, b, x[ 3], 14, 0xf4d50d87); MD5_GG(b, c, d, a, x[ 8], 20, 0x455a14ed);
    MD5_GG(a, b, c, d, x[13],  5, 0xa9e3e905); MD5_GG(d, a, b, c, x[ 2],  9, 0xfcefa3f8);
    MD5_GG(c, d, a, b, x[ 7], 14, 0x676f02d9); MD5_GG(b, c, d, a, x[12], 20, 0x8d2a4c8a);

    /* Round 3 */
    #define MD5_HH(a, b, c, d, x, s, ac) { \
        (a) += MD5_H((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = MD5_ROTATE_LEFT((a), (s)); \
        (a) += (b); \
    }
    MD5_HH(a, b, c, d, x[ 5],  4, 0xfffa3942); MD5_HH(d, a, b, c, x[ 8], 11, 0x8771f681);
    MD5_HH(c, d, a, b, x[11], 16, 0x6d9d6122); MD5_HH(b, c, d, a, x[14], 23, 0xfde5380c);
    MD5_HH(a, b, c, d, x[ 1],  4, 0xa4beea44); MD5_HH(d, a, b, c, x[ 4], 11, 0x4bdecfa9);
    MD5_HH(c, d, a, b, x[ 7], 16, 0xf6bb4b60); MD5_HH(b, c, d, a, x[10], 23, 0xbebfbc70);
    MD5_HH(a, b, c, d, x[13],  4, 0x289b7ec6); MD5_HH(d, a, b, c, x[ 0], 11, 0xeaa127fa);
    MD5_HH(c, d, a, b, x[ 3], 16, 0xd4ef3085); MD5_HH(b, c, d, a, x[ 6], 23, 0x04881d05);
    MD5_HH(a, b, c, d, x[ 9],  4, 0xd9d4d039); MD5_HH(d, a, b, c, x[12], 11, 0xe6db99e5);
    MD5_HH(c, d, a, b, x[15], 16, 0x1fa27cf8); MD5_HH(b, c, d, a, x[ 2], 23, 0xc4ac5665);

    /* Round 4 */
    #define MD5_II(a, b, c, d, x, s, ac) { \
        (a) += MD5_I((b), (c), (d)) + (x) + (uint32_t)(ac); \
        (a) = MD5_ROTATE_LEFT((a), (s)); \
        (a) += (b); \
    }
    MD5_II(a, b, c, d, x[ 0],  6, 0xf4292244); MD5_II(d, a, b, c, x[ 7], 10, 0x432aff97);
    MD5_II(c, d, a, b, x[14], 15, 0xab9423a7); MD5_II(b, c, d, a, x[ 5], 21, 0xfc93a039);
    MD5_II(a, b, c, d, x[12],  6, 0x655b59c3); MD5_II(d, a, b, c, x[ 3], 10, 0x8f0ccc92);
    MD5_II(c, d, a, b, x[10], 15, 0xffeff47d); MD5_II(b, c, d, a, x[ 1], 21, 0x85845dd1);
    MD5_II(a, b, c, d, x[ 8],  6, 0x6fa87e4f); MD5_II(d, a, b, c, x[15], 10, 0xfe2ce6e0);
    MD5_II(c, d, a, b, x[ 6], 15, 0xa3014314); MD5_II(b, c, d, a, x[13], 21, 0x4e0811a1);
    MD5_II(a, b, c, d, x[ 4],  6, 0xf7537e82); MD5_II(d, a, b, c, x[11], 10, 0xbd3af235);
    MD5_II(c, d, a, b, x[ 2], 15, 0x2ad7d2bb); MD5_II(b, c, d, a, x[ 9], 21, 0xeb86d391);

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
}

static void md5_init(crypto_md5_ctx_t *ctx) {
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xefcdab89;
    ctx->state[2] = 0x98badcfe;
    ctx->state[3] = 0x10325476;
    ctx->count = 0;
}

static void md5_update(crypto_md5_ctx_t *ctx, const uint8_t *data, size_t len) {
    size_t i, index, part_len;

    index = (size_t)((ctx->count >> 3) & 0x3F);
    ctx->count += (uint64_t)len << 3;
    part_len = 64 - index;

    if (len >= part_len) {
        memcpy(&ctx->buffer[index], data, part_len);
        md5_transform(ctx->state, ctx->buffer);
        for (i = part_len; i + 63 < len; i += 64) {
            md5_transform(ctx->state, &data[i]);
        }
        index = 0;
    } else {
        i = 0;
    }
    memcpy(&ctx->buffer[index], &data[i], len - i);
}

static void md5_final(crypto_md5_ctx_t *ctx, uint8_t digest[16]) {
    uint8_t bits[8];
    size_t index, pad_len;

    for (int i = 0; i < 8; i++) {
        bits[i] = (uint8_t)(ctx->count >> (i * 8));
    }

    index = (size_t)((ctx->count >> 3) & 0x3f);
    pad_len = (index < 56) ? (56 - index) : (120 - index);

    uint8_t padding[64] = {0x80};
    md5_update(ctx, padding, pad_len);
    md5_update(ctx, bits, 8);

    for (int i = 0; i < 4; i++) {
        digest[i*4+0] = (uint8_t)(ctx->state[i]);
        digest[i*4+1] = (uint8_t)(ctx->state[i] >> 8);
        digest[i*4+2] = (uint8_t)(ctx->state[i] >> 16);
        digest[i*4+3] = (uint8_t)(ctx->state[i] >> 24);
    }
}

/* ==================== SHA256 Implementation ==================== */

#define SHA256_ROTR(x, n) (((x) >> (n)) | ((x) << (32-(n))))
#define SHA256_CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define SHA256_MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA256_EP0(x) (SHA256_ROTR(x, 2) ^ SHA256_ROTR(x, 13) ^ SHA256_ROTR(x, 22))
#define SHA256_EP1(x) (SHA256_ROTR(x, 6) ^ SHA256_ROTR(x, 11) ^ SHA256_ROTR(x, 25))
#define SHA256_SIG0(x) (SHA256_ROTR(x, 7) ^ SHA256_ROTR(x, 18) ^ ((x) >> 3))
#define SHA256_SIG1(x) (SHA256_ROTR(x, 17) ^ SHA256_ROTR(x, 19) ^ ((x) >> 10))

static const uint32_t sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static void sha256_transform(uint32_t state[8], const uint8_t block[64]) {
    uint32_t a, b, c, d, e, f, g, h, t1, t2, m[64];

    for (int i = 0; i < 16; i++) {
        m[i] = ((uint32_t)block[i*4+0] << 24) | ((uint32_t)block[i*4+1] << 16) |
               ((uint32_t)block[i*4+2] << 8) | ((uint32_t)block[i*4+3]);
    }
    for (int i = 16; i < 64; i++) {
        m[i] = SHA256_SIG1(m[i-2]) + m[i-7] + SHA256_SIG0(m[i-15]) + m[i-16];
    }

    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];

    for (int i = 0; i < 64; i++) {
        t1 = h + SHA256_EP1(e) + SHA256_CH(e, f, g) + sha256_k[i] + m[i];
        t2 = SHA256_EP0(a) + SHA256_MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

static void sha256_init(crypto_sha256_ctx_t *ctx) {
    ctx->state[0] = 0x6a09e667; ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372; ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f; ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab; ctx->state[7] = 0x5be0cd19;
    ctx->count = 0;
}

static void sha256_update(crypto_sha256_ctx_t *ctx, const uint8_t *data, size_t len) {
    size_t i, index, part_len;

    index = (size_t)((ctx->count >> 3) & 0x3F);
    ctx->count += (uint64_t)len << 3;
    part_len = 64 - index;

    if (len >= part_len) {
        memcpy(&ctx->buffer[index], data, part_len);
        sha256_transform(ctx->state, ctx->buffer);
        for (i = part_len; i + 63 < len; i += 64) {
            sha256_transform(ctx->state, &data[i]);
        }
        index = 0;
    } else {
        i = 0;
    }
    memcpy(&ctx->buffer[index], &data[i], len - i);
}

static void sha256_final(crypto_sha256_ctx_t *ctx, uint8_t digest[32]) {
    uint8_t bits[8];
    size_t index, pad_len;

    for (int i = 0; i < 8; i++) {
        bits[7-i] = (uint8_t)(ctx->count >> (i * 8));
    }

    index = (size_t)((ctx->count >> 3) & 0x3f);
    pad_len = (index < 56) ? (56 - index) : (120 - index);

    uint8_t padding[64] = {0x80};
    sha256_update(ctx, padding, pad_len);
    sha256_update(ctx, bits, 8);

    for (int i = 0; i < 8; i++) {
        digest[i*4+0] = (uint8_t)(ctx->state[i] >> 24);
        digest[i*4+1] = (uint8_t)(ctx->state[i] >> 16);
        digest[i*4+2] = (uint8_t)(ctx->state[i] >> 8);
        digest[i*4+3] = (uint8_t)(ctx->state[i]);
    }
}

/* ==================== SHA512 Implementation ==================== */

#define SHA512_ROTR(x, n) (((x) >> (n)) | ((x) << (64-(n))))
#define SHA512_CH(x, y, z) (((x) & (y)) ^ (~(x) & (z)))
#define SHA512_MAJ(x, y, z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define SHA512_EP0(x) (SHA512_ROTR(x, 28) ^ SHA512_ROTR(x, 34) ^ SHA512_ROTR(x, 39))
#define SHA512_EP1(x) (SHA512_ROTR(x, 14) ^ SHA512_ROTR(x, 18) ^ SHA512_ROTR(x, 41))
#define SHA512_SIG0(x) (SHA512_ROTR(x, 1) ^ SHA512_ROTR(x, 8) ^ ((x) >> 7))
#define SHA512_SIG1(x) (SHA512_ROTR(x, 19) ^ SHA512_ROTR(x, 61) ^ ((x) >> 6))

static const uint64_t sha512_k[80] = {
    0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL,
    0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL, 0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL,
    0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
    0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL,
    0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL, 0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL,
    0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
    0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL,
    0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL, 0x06ca6351e003826fULL, 0x142929670a0e6e70ULL,
    0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
    0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL,
    0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL, 0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL,
    0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
    0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL,
    0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL, 0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL,
    0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
    0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL,
    0xca273eceea26619cULL, 0xd186b8c721c0c207ULL, 0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL,
    0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
    0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL,
    0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL, 0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL
};

static void sha512_transform(uint64_t state[8], const uint8_t block[128]) {
    uint64_t a, b, c, d, e, f, g, h, t1, t2, m[80];

    for (int i = 0; i < 16; i++) {
        m[i] = ((uint64_t)block[i*8+0] << 56) | ((uint64_t)block[i*8+1] << 48) |
               ((uint64_t)block[i*8+2] << 40) | ((uint64_t)block[i*8+3] << 32) |
               ((uint64_t)block[i*8+4] << 24) | ((uint64_t)block[i*8+5] << 16) |
               ((uint64_t)block[i*8+6] << 8) | ((uint64_t)block[i*8+7]);
    }
    for (int i = 16; i < 80; i++) {
        m[i] = SHA512_SIG1(m[i-2]) + m[i-7] + SHA512_SIG0(m[i-15]) + m[i-16];
    }

    a = state[0]; b = state[1]; c = state[2]; d = state[3];
    e = state[4]; f = state[5]; g = state[6]; h = state[7];

    for (int i = 0; i < 80; i++) {
        t1 = h + SHA512_EP1(e) + SHA512_CH(e, f, g) + sha512_k[i] + m[i];
        t2 = SHA512_EP0(a) + SHA512_MAJ(a, b, c);
        h = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }

    state[0] += a; state[1] += b; state[2] += c; state[3] += d;
    state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

static void sha512_init(crypto_sha512_ctx_t *ctx) {
    ctx->state[0] = 0x6a09e667f3bcc908ULL; ctx->state[1] = 0xbb67ae8584caa73bULL;
    ctx->state[2] = 0x3c6ef372fe94f82bULL; ctx->state[3] = 0xa54ff53a5f1d36f1ULL;
    ctx->state[4] = 0x510e527fade682d1ULL; ctx->state[5] = 0x9b05688c2b3e6c1fULL;
    ctx->state[6] = 0x1f83d9abfb41bd6bULL; ctx->state[7] = 0x5be0cd19137e2179ULL;
    ctx->count[0] = ctx->count[1] = 0;
}

static void sha512_update(crypto_sha512_ctx_t *ctx, const uint8_t *data, size_t len) {
    size_t i, index, part_len;

    index = (size_t)((ctx->count[0] >> 3) & 0x7F);

    if ((ctx->count[0] += ((uint64_t)len << 3)) < ((uint64_t)len << 3)) {
        ctx->count[1]++;
    }
    ctx->count[1] += ((uint64_t)len >> 61);

    part_len = 128 - index;

    if (len >= part_len) {
        memcpy(&ctx->buffer[index], data, part_len);
        sha512_transform(ctx->state, ctx->buffer);
        for (i = part_len; i + 127 < len; i += 128) {
            sha512_transform(ctx->state, &data[i]);
        }
        index = 0;
    } else {
        i = 0;
    }
    memcpy(&ctx->buffer[index], &data[i], len - i);
}

static void sha512_final(crypto_sha512_ctx_t *ctx, uint8_t digest[64]) {
    uint8_t bits[16];
    size_t index, pad_len;

    for (int i = 0; i < 8; i++) {
        bits[15-i] = (uint8_t)(ctx->count[0] >> (i * 8));
        bits[7-i] = (uint8_t)(ctx->count[1] >> (i * 8));
    }

    index = (size_t)((ctx->count[0] >> 3) & 0x7f);
    pad_len = (index < 112) ? (112 - index) : (240 - index);

    uint8_t padding[128] = {0x80};
    sha512_update(ctx, padding, pad_len);
    sha512_update(ctx, bits, 16);

    for (int i = 0; i < 8; i++) {
        digest[i*8+0] = (uint8_t)(ctx->state[i] >> 56);
        digest[i*8+1] = (uint8_t)(ctx->state[i] >> 48);
        digest[i*8+2] = (uint8_t)(ctx->state[i] >> 40);
        digest[i*8+3] = (uint8_t)(ctx->state[i] >> 32);
        digest[i*8+4] = (uint8_t)(ctx->state[i] >> 24);
        digest[i*8+5] = (uint8_t)(ctx->state[i] >> 16);
        digest[i*8+6] = (uint8_t)(ctx->state[i] >> 8);
        digest[i*8+7] = (uint8_t)(ctx->state[i]);
    }
}

/* ==================== Generic Hash Interface ==================== */

size_t crypto_hash_digest_size(crypto_hash_type_t type) {
    switch (type) {
        case CRYPTO_HASH_MD5: return CRYPTO_MD5_DIGEST_SIZE;
        case CRYPTO_HASH_SHA1: return CRYPTO_SHA1_DIGEST_SIZE;
        case CRYPTO_HASH_SHA256: return CRYPTO_SHA256_DIGEST_SIZE;
        case CRYPTO_HASH_SHA512: return CRYPTO_SHA512_DIGEST_SIZE;
        default: return 0;
    }
}

const char* crypto_hash_type_name(crypto_hash_type_t type) {
    switch (type) {
        case CRYPTO_HASH_MD5: return "md5";
        case CRYPTO_HASH_SHA1: return "sha1";
        case CRYPTO_HASH_SHA256: return "sha256";
        case CRYPTO_HASH_SHA512: return "sha512";
        default: return "unknown";
    }
}

crypto_hash_t* crypto_hash_create(crypto_hash_type_t type) {
    crypto_hash_t *hash = (crypto_hash_t*)malloc(sizeof(crypto_hash_t));
    if (!hash) {
        COSMORUN_ERROR_NULL(COSMORUN_ERR_OUT_OF_MEMORY, "Failed to allocate hash context");
    }

    hash->type = type;

    switch (type) {
        case CRYPTO_HASH_MD5:
            md5_init(&hash->ctx.md5);
            break;
        case CRYPTO_HASH_SHA256:
            sha256_init(&hash->ctx.sha256);
            break;
        case CRYPTO_HASH_SHA512:
            sha512_init(&hash->ctx.sha512);
            break;
        default:
            free(hash);
            COSMORUN_ERROR_NULL(COSMORUN_ERR_INVALID_ARG, "Invalid hash type");
    }

    return hash;
}

int crypto_hash_update(crypto_hash_t *hash, const uint8_t *data, size_t len) {
    if (!hash || !data) {
        COSMORUN_ERROR_RETURN(COSMORUN_ERR_INVALID_ARG, "Invalid arguments to crypto_hash_update", -1);
    }

    switch (hash->type) {
        case CRYPTO_HASH_MD5:
            md5_update(&hash->ctx.md5, data, len);
            break;
        case CRYPTO_HASH_SHA256:
            sha256_update(&hash->ctx.sha256, data, len);
            break;
        case CRYPTO_HASH_SHA512:
            sha512_update(&hash->ctx.sha512, data, len);
            break;
        default:
            COSMORUN_ERROR_RETURN(COSMORUN_ERR_INVALID_ARG, "Invalid hash type", -1);
    }

    return 0;
}

int crypto_hash_final(crypto_hash_t *hash, uint8_t *output) {
    if (!hash || !output) {
        COSMORUN_ERROR_RETURN(COSMORUN_ERR_INVALID_ARG, "Invalid arguments to crypto_hash_final", -1);
    }

    switch (hash->type) {
        case CRYPTO_HASH_MD5:
            md5_final(&hash->ctx.md5, output);
            break;
        case CRYPTO_HASH_SHA256:
            sha256_final(&hash->ctx.sha256, output);
            break;
        case CRYPTO_HASH_SHA512:
            sha512_final(&hash->ctx.sha512, output);
            break;
        default:
            COSMORUN_ERROR_RETURN(COSMORUN_ERR_INVALID_ARG, "Invalid hash type", -1);
    }

    return 0;
}

void crypto_hash_free(crypto_hash_t *hash) {
    if (hash) {
        crypto_secure_zero(hash, sizeof(crypto_hash_t));
        free(hash);
    }
}

int crypto_hash_simple(crypto_hash_type_t type, const uint8_t *data, size_t len, uint8_t *output) {
    crypto_hash_t *hash = crypto_hash_create(type);
    if (!hash) return -1;

    int ret = crypto_hash_update(hash, data, len);
    if (ret == 0) {
        ret = crypto_hash_final(hash, output);
    }

    crypto_hash_free(hash);
    return ret;
}

/* ==================== HMAC Implementation ==================== */

crypto_hmac_t* crypto_hmac_create(crypto_hash_type_t type, const uint8_t *key, size_t key_len) {
    crypto_hmac_t *hmac = (crypto_hmac_t*)malloc(sizeof(crypto_hmac_t));
    if (!hmac) {
        COSMORUN_ERROR_NULL(COSMORUN_ERR_OUT_OF_MEMORY, "Failed to allocate HMAC context");
    }

    hmac->hash_type = type;
    hmac->block_size = (type == CRYPTO_HASH_SHA512) ? 128 : 64;

    uint8_t key_buffer[128] = {0};

    if (key_len > hmac->block_size) {
        crypto_hash_simple(type, key, key_len, key_buffer);
        key_len = crypto_hash_digest_size(type);
    } else {
        memcpy(key_buffer, key, key_len);
    }

    for (size_t i = 0; i < hmac->block_size; i++) {
        hmac->key_pad[i] = key_buffer[i] ^ 0x36;
    }

    crypto_hash_t *hash = crypto_hash_create(type);
    if (!hash) {
        free(hmac);
        COSMORUN_ERROR_NULL(COSMORUN_ERR_OUT_OF_MEMORY, "Failed to create hash for HMAC");
    }

    crypto_hash_update(hash, hmac->key_pad, hmac->block_size);
    memcpy(&hmac->hash, hash, sizeof(crypto_hash_t));
    crypto_hash_free(hash);

    for (size_t i = 0; i < hmac->block_size; i++) {
        hmac->key_pad[i] ^= 0x36 ^ 0x5c;
    }

    crypto_secure_zero(key_buffer, sizeof(key_buffer));
    return hmac;
}

int crypto_hmac_update(crypto_hmac_t *hmac, const uint8_t *data, size_t len) {
    if (!hmac || !data) {
        COSMORUN_ERROR_RETURN(COSMORUN_ERR_INVALID_ARG, "Invalid arguments to crypto_hmac_update", -1);
    }
    return crypto_hash_update(&hmac->hash, data, len);
}

int crypto_hmac_final(crypto_hmac_t *hmac, uint8_t *output) {
    if (!hmac || !output) {
        COSMORUN_ERROR_RETURN(COSMORUN_ERR_INVALID_ARG, "Invalid arguments to crypto_hmac_final", -1);
    }

    uint8_t inner_hash[CRYPTO_MAX_DIGEST_SIZE];
    crypto_hash_final(&hmac->hash, inner_hash);

    crypto_hash_t *outer = crypto_hash_create(hmac->hash_type);
    if (!outer) {
        COSMORUN_ERROR_RETURN(COSMORUN_ERR_OUT_OF_MEMORY, "Failed to create outer hash for HMAC", -1);
    }

    crypto_hash_update(outer, hmac->key_pad, hmac->block_size);
    crypto_hash_update(outer, inner_hash, crypto_hash_digest_size(hmac->hash_type));
    crypto_hash_final(outer, output);

    crypto_hash_free(outer);
    crypto_secure_zero(inner_hash, sizeof(inner_hash));

    return 0;
}

void crypto_hmac_free(crypto_hmac_t *hmac) {
    if (hmac) {
        crypto_secure_zero(hmac, sizeof(crypto_hmac_t));
        free(hmac);
    }
}

int crypto_hmac_simple(crypto_hash_type_t type, const uint8_t *key, size_t key_len,
                       const uint8_t *data, size_t data_len, uint8_t *output) {
    crypto_hmac_t *hmac = crypto_hmac_create(type, key, key_len);
    if (!hmac) return -1;

    int ret = crypto_hmac_update(hmac, data, data_len);
    if (ret == 0) {
        ret = crypto_hmac_final(hmac, output);
    }

    crypto_hmac_free(hmac);
    return ret;
}

/* ==================== Random Number Generation ==================== */

int crypto_random_bytes(uint8_t *buffer, size_t len) {
    FILE *fp = fopen("/dev/urandom", "rb");
    if (!fp) return -1;

    size_t read = fread(buffer, 1, len, fp);
    fclose(fp);

    return (read == len) ? 0 : -1;
}

/* ==================== Hex Encoding ==================== */

char* crypto_hex_encode(const uint8_t *data, size_t len) {
    char *hex = (char*)malloc(len * 2 + 1);
    if (!hex) return NULL;

    for (size_t i = 0; i < len; i++) {
        sprintf(&hex[i*2], "%02x", data[i]);
    }
    hex[len * 2] = '\0';

    return hex;
}

int crypto_hex_decode(const char *hex, uint8_t *output, size_t *output_len) {
    if (!hex || !output || !output_len) return -1;

    size_t len = strlen(hex);
    if (len % 2 != 0) return -1;

    *output_len = len / 2;

    for (size_t i = 0; i < *output_len; i++) {
        char h1 = hex[i*2];
        char h2 = hex[i*2+1];

        int v1 = (h1 >= '0' && h1 <= '9') ? h1 - '0' :
                 (h1 >= 'a' && h1 <= 'f') ? h1 - 'a' + 10 :
                 (h1 >= 'A' && h1 <= 'F') ? h1 - 'A' + 10 : -1;
        int v2 = (h2 >= '0' && h2 <= '9') ? h2 - '0' :
                 (h2 >= 'a' && h2 <= 'f') ? h2 - 'a' + 10 :
                 (h2 >= 'A' && h2 <= 'F') ? h2 - 'A' + 10 : -1;

        if (v1 < 0 || v2 < 0) return -1;

        output[i] = (uint8_t)((v1 << 4) | v2);
    }

    return 0;
}

/* ==================== PKCS7 Padding ==================== */

int crypto_pkcs7_pad(const uint8_t *input, size_t input_len,
                     uint8_t *output, size_t *output_len, size_t block_size) {
    if (!input || !output || !output_len || block_size > 255) return -1;

    size_t pad_len = block_size - (input_len % block_size);
    *output_len = input_len + pad_len;

    memcpy(output, input, input_len);
    for (size_t i = 0; i < pad_len; i++) {
        output[input_len + i] = (uint8_t)pad_len;
    }

    return 0;
}

int crypto_pkcs7_unpad(const uint8_t *input, size_t input_len,
                       uint8_t *output, size_t *output_len) {
    if (!input || !output || !output_len || input_len == 0) return -1;

    uint8_t pad_len = input[input_len - 1];
    if (pad_len == 0 || pad_len > input_len) return -1;

    for (size_t i = input_len - pad_len; i < input_len; i++) {
        if (input[i] != pad_len) return -1;
    }

    *output_len = input_len - pad_len;
    memcpy(output, input, *output_len);

    return 0;
}

/* ==================== AES Implementation (Stub) ==================== */

crypto_cipher_t* crypto_cipher_create(crypto_cipher_type_t type,
                                       const uint8_t *key, const uint8_t *iv) {
    /* AES implementation would go here - this is a stub for now */
    return NULL;
}

int crypto_cipher_encrypt(crypto_cipher_t *cipher, const uint8_t *input,
                          size_t input_len, uint8_t *output, size_t *output_len) {
    /* AES encryption would go here - this is a stub for now */
    return -1;
}

int crypto_cipher_decrypt(crypto_cipher_t *cipher, const uint8_t *input,
                          size_t input_len, uint8_t *output, size_t *output_len) {
    /* AES decryption would go here - this is a stub for now */
    return -1;
}

void crypto_cipher_free(crypto_cipher_t *cipher) {
    if (cipher) {
        crypto_secure_zero(cipher, sizeof(crypto_cipher_t));
        free(cipher);
    }
}
