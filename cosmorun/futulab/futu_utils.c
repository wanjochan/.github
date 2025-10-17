/*
 * Futu OpenD API Utilities - Implementation
 *
 * Common utilities for Futu OpenD protocol communication
 */

#include "futu_utils.h"
extern int socket(int domain, int type, int protocol);
extern uint16_t htons(uint16_t hostshort);
extern ssize_t send(int sockfd, const void *buf, size_t len, int flags);
extern ssize_t recv(int sockfd, void *buf, size_t len, int flags);
/* ========== Protobuf Encoding Functions ========== */

/* Encode varint (variable-length integer) */
size_t encode_varint(uint8_t *buf, uint64_t value) {
    size_t pos = 0;
    while (value >= 0x80) {
        buf[pos++] = (uint8_t)((value & 0x7F) | 0x80);
        value >>= 7;
    }
    buf[pos++] = (uint8_t)(value & 0x7F);
    return pos;
}

/* Encode field key (field_number << 3 | wire_type) */
size_t encode_field_key(uint8_t *buf, uint32_t field_number, uint32_t wire_type) {
    return encode_varint(buf, (field_number << 3) | wire_type);
}

/* Encode int32 field */
size_t encode_int32(uint8_t *buf, uint32_t field_number, int32_t value) {
    size_t pos = 0;
    pos += encode_field_key(buf + pos, field_number, 0);  /* wire_type=0 for varint */
    pos += encode_varint(buf + pos, (uint64_t)(uint32_t)value);
    return pos;
}

/* Encode uint64 field */
size_t encode_uint64(uint8_t *buf, uint32_t field_number, uint64_t value) {
    size_t pos = 0;
    pos += encode_field_key(buf + pos, field_number, 0);  /* wire_type=0 for varint */
    pos += encode_varint(buf + pos, value);
    return pos;
}

/* Encode bool field */
size_t encode_bool(uint8_t *buf, uint32_t field_number, int value) {
    size_t pos = 0;
    pos += encode_field_key(buf + pos, field_number, 0);  /* wire_type=0 for varint */
    pos += encode_varint(buf + pos, value ? 1 : 0);
    return pos;
}

/* Encode string field */
size_t encode_string(uint8_t *buf, uint32_t field_number, const char *str) {
    size_t pos = 0;
    size_t len = strlen(str);
    pos += encode_field_key(buf + pos, field_number, 2);  /* wire_type=2 for length-delimited */
    pos += encode_varint(buf + pos, len);
    memcpy(buf + pos, str, len);
    pos += len;
    return pos;
}

/* Encode embedded message field */
size_t encode_message(uint8_t *buf, uint32_t field_number, const uint8_t *msg, size_t msg_len) {
    size_t pos = 0;
    pos += encode_field_key(buf + pos, field_number, 2);  /* wire_type=2 for length-delimited */
    pos += encode_varint(buf + pos, msg_len);
    memcpy(buf + pos, msg, msg_len);
    pos += msg_len;
    return pos;
}

/* ========== Protobuf Decoding Functions ========== */

/* Decode varint */
size_t decode_varint(const uint8_t *buf, uint64_t *value) {
    size_t pos = 0;
    uint64_t result = 0;
    int shift = 0;

    while (1) {
        uint8_t b = buf[pos++];
        result |= (uint64_t)(b & 0x7F) << shift;
        if ((b & 0x80) == 0) break;
        shift += 7;
    }

    *value = result;
    return pos;
}

/* Decode field key */
size_t decode_field_key(const uint8_t *buf, uint32_t *field_number, uint32_t *wire_type) {
    uint64_t key;
    size_t pos = decode_varint(buf, &key);
    *field_number = (uint32_t)(key >> 3);
    *wire_type = (uint32_t)(key & 0x7);
    return pos;
}

/* Skip field based on wire type */
size_t skip_field(const uint8_t *buf, uint32_t wire_type) {
    size_t pos = 0;
    uint64_t len;

    switch (wire_type) {
        case 0:  /* Varint */
            pos += decode_varint(buf + pos, &len);
            break;
        case 1:  /* 64-bit */
            pos += 8;
            break;
        case 2:  /* Length-delimited */
            pos += decode_varint(buf + pos, &len);
            pos += len;
            break;
        case 5:  /* 32-bit */
            pos += 4;
            break;
        default:
            fprintf(stderr, "Unknown wire type: %u\n", wire_type);
            break;
    }

    return pos;
}

/* ========== SHA1 Implementation ========== */

typedef struct {
    uint32_t state[5];
    uint32_t count[2];
    uint8_t buffer[64];
} sha1_ctx_t;

#define SHA1_ROL(value, bits) (((value) << (bits)) | ((value) >> (32 - (bits))))

static void sha1_transform(uint32_t state[5], const uint8_t buffer[64]) {
    uint32_t a, b, c, d, e;
    uint32_t w[80];
    int i;

    /* Prepare message schedule */
    for (i = 0; i < 16; i++) {
        w[i] = ((uint32_t)buffer[i * 4] << 24) |
               ((uint32_t)buffer[i * 4 + 1] << 16) |
               ((uint32_t)buffer[i * 4 + 2] << 8) |
               (uint32_t)buffer[i * 4 + 3];
    }
    for (i = 16; i < 80; i++) {
        w[i] = SHA1_ROL(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
    }

    /* Initialize working variables */
    a = state[0];
    b = state[1];
    c = state[2];
    d = state[3];
    e = state[4];

    /* Main loop */
    for (i = 0; i < 80; i++) {
        uint32_t f, k, temp;
        if (i < 20) {
            f = (b & c) | ((~b) & d);
            k = 0x5A827999;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }
        temp = SHA1_ROL(a, 5) + f + e + k + w[i];
        e = d;
        d = c;
        c = SHA1_ROL(b, 30);
        b = a;
        a = temp;
    }

    /* Add this chunk's hash to result */
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}

static void sha1_init(sha1_ctx_t *ctx) {
    ctx->state[0] = 0x67452301;
    ctx->state[1] = 0xEFCDAB89;
    ctx->state[2] = 0x98BADCFE;
    ctx->state[3] = 0x10325476;
    ctx->state[4] = 0xC3D2E1F0;
    ctx->count[0] = ctx->count[1] = 0;
}

static void sha1_update(sha1_ctx_t *ctx, const uint8_t *data, uint32_t len) {
    uint32_t i, j;

    j = (ctx->count[0] >> 3) & 63;
    if ((ctx->count[0] += len << 3) < (len << 3)) ctx->count[1]++;
    ctx->count[1] += (len >> 29);

    if ((j + len) > 63) {
        i = 64 - j;
        memcpy(&ctx->buffer[j], data, i);
        sha1_transform(ctx->state, ctx->buffer);
        for (; i + 63 < len; i += 64) {
            sha1_transform(ctx->state, &data[i]);
        }
        j = 0;
    } else {
        i = 0;
    }
    memcpy(&ctx->buffer[j], &data[i], len - i);
}

static void sha1_final(uint8_t digest[20], sha1_ctx_t *ctx) {
    uint32_t i;
    uint8_t finalcount[8];

    for (i = 0; i < 8; i++) {
        finalcount[i] = (uint8_t)((ctx->count[(i >= 4 ? 0 : 1)] >>
                                   ((3 - (i & 3)) * 8)) & 255);
    }

    sha1_update(ctx, (const uint8_t *)"\x80", 1);
    while ((ctx->count[0] & 504) != 448) {
        sha1_update(ctx, (const uint8_t *)"\0", 1);
    }
    sha1_update(ctx, finalcount, 8);

    for (i = 0; i < 20; i++) {
        digest[i] = (uint8_t)((ctx->state[i >> 2] >> ((3 - (i & 3)) * 8)) & 255);
    }
}

void sha1_hash(const uint8_t *data, uint32_t len, uint8_t digest[20]) {
    sha1_ctx_t ctx;
    sha1_init(&ctx);
    sha1_update(&ctx, data, len);
    sha1_final(digest, &ctx);
}

/* ========== Futu Protocol Functions ========== */

/* Build Futu protocol header */
void build_futu_header(futu_header_t *header, uint32_t proto_id,
                       uint32_t serial_no, const uint8_t *body, uint32_t body_len) {
    memset(header, 0, sizeof(futu_header_t));
    header->header_flag[0] = 'F';
    header->header_flag[1] = 'T';

    /* Protocol ID (little-endian for x86/x64) */
    header->proto_id[0] = proto_id & 0xFF;
    header->proto_id[1] = (proto_id >> 8) & 0xFF;
    header->proto_id[2] = (proto_id >> 16) & 0xFF;
    header->proto_id[3] = (proto_id >> 24) & 0xFF;

    header->proto_fmt_type = FUTU_PROTO_FMT_PROTOBUF;
    header->proto_ver = 0;

    /* Serial number (little-endian) */
    header->serial_no[0] = serial_no & 0xFF;
    header->serial_no[1] = (serial_no >> 8) & 0xFF;
    header->serial_no[2] = (serial_no >> 16) & 0xFF;
    header->serial_no[3] = (serial_no >> 24) & 0xFF;

    /* Body length (little-endian) */
    header->body_len[0] = body_len & 0xFF;
    header->body_len[1] = (body_len >> 8) & 0xFF;
    header->body_len[2] = (body_len >> 16) & 0xFF;
    header->body_len[3] = (body_len >> 24) & 0xFF;

    /* Calculate SHA1 hash of body */
    if (body_len > 0 && body != NULL) {
        sha1_hash(body, body_len, header->body_sha1);
    } else {
        memset(header->body_sha1, 0, 20);
    }
}

/* Connect to Futu OpenD */
int connect_to_opend(const char *host, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    /* Set receive timeout to 5 seconds */
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt SO_RCVTIMEO");
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    return sock;
}

/* Send request and receive response */
int send_request(int sock, uint32_t proto_id, const uint8_t *body, uint32_t body_len,
                uint8_t *resp_buf, size_t resp_buf_size) {
    /* Build header with SHA1 hash */
    futu_header_t header;
    static uint32_t serial_no = 1;
    build_futu_header(&header, proto_id, serial_no++, body, body_len);

    /* Send header and body together for atomicity */
    uint8_t send_buf[FUTU_HEADER_SIZE + 4096];
    if (FUTU_HEADER_SIZE + body_len > sizeof(send_buf)) {
        fprintf(stderr, "Request too large\n");
        return -1;
    }

    memcpy(send_buf, &header, FUTU_HEADER_SIZE);
    if (body_len > 0) {
        memcpy(send_buf + FUTU_HEADER_SIZE, body, body_len);
    }

    ssize_t total_len = FUTU_HEADER_SIZE + body_len;
    if (send(sock, send_buf, total_len, 0) != total_len) {
        perror("send request");
        return -1;
    }

    /* Receive response header */
    futu_header_t resp_header;
    ssize_t n = recv(sock, &resp_header, sizeof(resp_header), 0);
    if (n != sizeof(resp_header)) {
        if (n < 0) {
            perror("recv header");
        } else if (n == 0) {
            fprintf(stderr, "Connection closed by server\n");
        } else {
            fprintf(stderr, "Partial header received: %zd bytes\n", n);
        }
        return -1;
    }

    /* Parse response header (little-endian) */
    uint32_t resp_body_len = (uint32_t)resp_header.body_len[0] |
                             ((uint32_t)resp_header.body_len[1] << 8) |
                             ((uint32_t)resp_header.body_len[2] << 16) |
                             ((uint32_t)resp_header.body_len[3] << 24);

    if (resp_body_len > resp_buf_size) {
        fprintf(stderr, "Response too large: %u bytes\n", resp_body_len);
        return -1;
    }

    /* Receive response body */
    if (resp_body_len > 0) {
        n = recv(sock, resp_buf, resp_body_len, 0);
        if (n != (ssize_t)resp_body_len) {
            fprintf(stderr, "recv body failed\n");
            return -1;
        }
    }

    return (int)resp_body_len;
}

/* Parse common Response wrapper */
int parse_response_header(const uint8_t *buf, size_t buf_len,
                          int32_t *ret_type, const uint8_t **s2c_data, size_t *s2c_len) {
    size_t pos = 0;
    *ret_type = -400;
    *s2c_data = NULL;
    *s2c_len = 0;

    while (pos < buf_len) {
        uint32_t field_number, wire_type;
        pos += decode_field_key(buf + pos, &field_number, &wire_type);

        if (field_number == 1) {  /* retType */
            uint64_t val;
            pos += decode_varint(buf + pos, &val);
            *ret_type = (int32_t)val;
        } else if (field_number == 2) {  /* retMsg */
            uint64_t len;
            pos += decode_varint(buf + pos, &len);
            printf("  retMsg: %.*s\n", (int)len, buf + pos);
            pos += len;
        } else if (field_number == 4) {  /* s2c */
            uint64_t len;
            pos += decode_varint(buf + pos, &len);
            *s2c_data = buf + pos;
            *s2c_len = len;
            pos += len;
        } else {
            pos += skip_field(buf + pos, wire_type);
        }
    }

    return 0;
}

/* ========== Security Encoding (for Quote APIs) ========== */

/* Encode Security message (market + code) */
size_t encode_security(uint8_t *buf, int32_t market, const char *code) {
    size_t pos = 0;
    pos += encode_int32(buf + pos, 1, market);  /* market field */
    pos += encode_string(buf + pos, 2, code);   /* code field */
    return pos;
}

/* ========== Utility Functions ========== */

/* Print hex dump of buffer (for debugging) */
void print_hex(const char *label, const uint8_t *data, size_t len) {
    printf("%s: ", label);
    for (size_t i = 0; i < len && i < 32; i++) {
        printf("%02x ", data[i]);
    }
    if (len > 32) printf("...");
    printf("\n");
}

/* ========== Quote API Helper Functions ========== */

/* Build Qot_RequestHistoryKL request */
size_t build_request_history_kl(uint8_t *buf, int32_t market, const char *code,
                                int32_t kl_type, int32_t rehab_type,
                                const char *begin_time, const char *end_time,
                                int32_t max_count) {
    /* Build Security message */
    uint8_t security_buf[128];
    size_t security_len = encode_security(security_buf, market, code);

    /* Build C2S message - Field order per Qot_RequestHistoryKL.proto */
    uint8_t c2s_buf[512];
    size_t c2s_len = 0;

    /* Field 1: rehabType (required) */
    c2s_len += encode_int32(c2s_buf + c2s_len, 1, rehab_type);
    /* Field 2: klType (required) */
    c2s_len += encode_int32(c2s_buf + c2s_len, 2, kl_type);
    /* Field 3: security (required) */
    c2s_len += encode_message(c2s_buf + c2s_len, 3, security_buf, security_len);
    /* Field 4: beginTime (required) - use empty string if not provided */
    c2s_len += encode_string(c2s_buf + c2s_len, 4, (begin_time && begin_time[0]) ? begin_time : "");
    /* Field 5: endTime (required) - use empty string if not provided */
    c2s_len += encode_string(c2s_buf + c2s_len, 5, (end_time && end_time[0]) ? end_time : "");
    /* Field 6: maxAckKLNum (optional) */
    if (max_count > 0) {
        c2s_len += encode_int32(c2s_buf + c2s_len, 6, max_count);
    }

    /* Build Request wrapper */
    size_t req_len = 0;
    req_len += encode_message(buf + req_len, 1, c2s_buf, c2s_len);

    return req_len;
}

/* Build Qot_GetOrderBook request */
size_t build_get_order_book(uint8_t *buf, int32_t market, const char *code, int32_t num) {
    /* Build Security message */
    uint8_t security_buf[128];
    size_t security_len = encode_security(security_buf, market, code);

    /* Build C2S message */
    uint8_t c2s_buf[256];
    size_t c2s_len = 0;

    /* Field 1: security */
    c2s_len += encode_message(c2s_buf + c2s_len, 1, security_buf, security_len);
    /* Field 2: num (number of order book levels) */
    c2s_len += encode_int32(c2s_buf + c2s_len, 2, num);

    /* Build Request wrapper */
    size_t req_len = 0;
    req_len += encode_message(buf + req_len, 1, c2s_buf, c2s_len);

    return req_len;
}

/* ========== Trade API Helper Functions ========== */

/* Build Trd_GetAccList request */
size_t build_get_acc_list(uint8_t *buf, uint64_t user_id) {
    /* Build C2S message */
    uint8_t c2s_buf[64];
    size_t c2s_len = 0;

    /* Field 1: userID */
    c2s_len += encode_uint64(c2s_buf + c2s_len, 1, user_id);

    /* Build Request wrapper */
    size_t req_len = 0;
    req_len += encode_message(buf + req_len, 1, c2s_buf, c2s_len);

    return req_len;
}

/* Build Trd_UnlockTrade request */
size_t build_unlock_trade(uint8_t *buf, const char *password, int is_unlock) {
    /* Build C2S message */
    uint8_t c2s_buf[256];
    size_t c2s_len = 0;

    /* Field 1: unlock (true to unlock, false to lock) */
    c2s_len += encode_bool(c2s_buf + c2s_len, 1, is_unlock);
    /* Field 2: pwdMD5 (password MD5 hash) */
    if (password && password[0]) {
        c2s_len += encode_string(c2s_buf + c2s_len, 2, password);
    }

    /* Build Request wrapper */
    size_t req_len = 0;
    req_len += encode_message(buf + req_len, 1, c2s_buf, c2s_len);

    return req_len;
}

/* Build Trd_GetFunds request */
size_t build_get_funds(uint8_t *buf, uint64_t acc_id, int32_t trd_market, int refresh_cache) {
    /* Build Header (TrdHeader) */
    uint8_t header_buf[128];
    size_t header_len = 0;

    /* Field 1: trdEnv */
    header_len += encode_int32(header_buf + header_len, 1, TRD_ENV_REAL);
    /* Field 2: accID */
    header_len += encode_uint64(header_buf + header_len, 2, acc_id);
    /* Field 3: trdMarket */
    header_len += encode_int32(header_buf + header_len, 3, trd_market);

    /* Build C2S message */
    uint8_t c2s_buf[256];
    size_t c2s_len = 0;

    /* Field 1: header */
    c2s_len += encode_message(c2s_buf + c2s_len, 1, header_buf, header_len);
    /* Field 2: refreshCache */
    if (refresh_cache) {
        c2s_len += encode_bool(c2s_buf + c2s_len, 2, 1);
    }

    /* Build Request wrapper */
    size_t req_len = 0;
    req_len += encode_message(buf + req_len, 1, c2s_buf, c2s_len);

    return req_len;
}

/* Build Trd_GetPositionList request */
size_t build_get_position_list(uint8_t *buf, uint64_t acc_id, int32_t trd_market, const char *code) {
    /* Build Header (TrdHeader) */
    uint8_t header_buf[128];
    size_t header_len = 0;

    header_len += encode_int32(header_buf + header_len, 1, TRD_ENV_REAL);
    header_len += encode_uint64(header_buf + header_len, 2, acc_id);
    header_len += encode_int32(header_buf + header_len, 3, trd_market);

    /* Build C2S message */
    uint8_t c2s_buf[512];
    size_t c2s_len = 0;

    /* Field 1: header */
    c2s_len += encode_message(c2s_buf + c2s_len, 1, header_buf, header_len);

    /* Field 2: filterConditions (optional - filter by code) */
    if (code && code[0]) {
        /* Build TrdFilterConditions */
        uint8_t filter_buf[256];
        size_t filter_len = 0;

        /* Field 1: codeList */
        filter_len += encode_string(filter_buf + filter_len, 1, code);

        c2s_len += encode_message(c2s_buf + c2s_len, 2, filter_buf, filter_len);
    }

    /* Build Request wrapper */
    size_t req_len = 0;
    req_len += encode_message(buf + req_len, 1, c2s_buf, c2s_len);

    return req_len;
}

/* Build Trd_PlaceOrder request */
size_t build_place_order(uint8_t *buf, uint64_t acc_id, int32_t trd_market,
                        int32_t trd_side, int32_t order_type,
                        const char *code, double price, double qty) {
    /* Build Header (TrdHeader) */
    uint8_t header_buf[128];
    size_t header_len = 0;

    header_len += encode_int32(header_buf + header_len, 1, TRD_ENV_REAL);
    header_len += encode_uint64(header_buf + header_len, 2, acc_id);
    header_len += encode_int32(header_buf + header_len, 3, trd_market);

    /* Build C2S message */
    uint8_t c2s_buf[512];
    size_t c2s_len = 0;

    /* Field 1: packetID (optional) */
    /* Field 2: header */
    c2s_len += encode_message(c2s_buf + c2s_len, 2, header_buf, header_len);
    /* Field 3: trdSide */
    c2s_len += encode_int32(c2s_buf + c2s_len, 3, trd_side);
    /* Field 4: orderType */
    c2s_len += encode_int32(c2s_buf + c2s_len, 4, order_type);
    /* Field 5: code */
    c2s_len += encode_string(c2s_buf + c2s_len, 5, code);
    /* Field 6: qty */
    c2s_len += encode_field_key(c2s_buf + c2s_len, 6, 1);  /* wire_type=1 for double */
    memcpy(c2s_buf + c2s_len, &qty, 8);
    c2s_len += 8;
    /* Field 7: price */
    if (order_type != ORDER_TYPE_MARKET) {
        c2s_len += encode_field_key(c2s_buf + c2s_len, 7, 1);  /* wire_type=1 for double */
        memcpy(c2s_buf + c2s_len, &price, 8);
        c2s_len += 8;
    }

    /* Build Request wrapper */
    size_t req_len = 0;
    req_len += encode_message(buf + req_len, 1, c2s_buf, c2s_len);

    return req_len;
}

/* Build Trd_ModifyOrder request */
size_t build_modify_order(uint8_t *buf, uint64_t acc_id, int32_t trd_market,
                         uint64_t order_id, int32_t modify_op,
                         double price, double qty) {
    /* Build Header (TrdHeader) */
    uint8_t header_buf[128];
    size_t header_len = 0;

    header_len += encode_int32(header_buf + header_len, 1, TRD_ENV_REAL);
    header_len += encode_uint64(header_buf + header_len, 2, acc_id);
    header_len += encode_int32(header_buf + header_len, 3, trd_market);

    /* Build C2S message - Field order per Trd_ModifyOrder.proto */
    uint8_t c2s_buf[512];
    size_t c2s_len = 0;

    /* Field 1: packetID (optional, skip) */
    /* Field 2: header */
    c2s_len += encode_message(c2s_buf + c2s_len, 2, header_buf, header_len);
    /* Field 3: orderID */
    c2s_len += encode_uint64(c2s_buf + c2s_len, 3, order_id);
    /* Field 4: modifyOrderOp */
    c2s_len += encode_int32(c2s_buf + c2s_len, 4, modify_op);
    /* Field 8: qty (optional, only for MODIFY operation) */
    if (modify_op == MODIFY_ORDER_OP_MODIFY && qty > 0) {
        c2s_len += encode_field_key(c2s_buf + c2s_len, 8, 1);  /* wire_type=1 for double */
        memcpy(c2s_buf + c2s_len, &qty, 8);
        c2s_len += 8;
    }
    /* Field 9: price (optional, only for MODIFY operation) */
    if (modify_op == MODIFY_ORDER_OP_MODIFY && price > 0) {
        c2s_len += encode_field_key(c2s_buf + c2s_len, 9, 1);  /* wire_type=1 for double */
        memcpy(c2s_buf + c2s_len, &price, 8);
        c2s_len += 8;
    }

    /* Build Request wrapper */
    size_t req_len = 0;
    req_len += encode_message(buf + req_len, 1, c2s_buf, c2s_len);

    return req_len;
}

/* Build Trd_GetOrderList request */
size_t build_get_order_list(uint8_t *buf, uint64_t acc_id, int32_t trd_market,
                           int refresh_cache) {
    /* Build Header (TrdHeader) */
    uint8_t header_buf[128];
    size_t header_len = 0;

    header_len += encode_int32(header_buf + header_len, 1, TRD_ENV_REAL);
    header_len += encode_uint64(header_buf + header_len, 2, acc_id);
    header_len += encode_int32(header_buf + header_len, 3, trd_market);

    /* Build C2S message */
    uint8_t c2s_buf[256];
    size_t c2s_len = 0;

    /* Field 1: header */
    c2s_len += encode_message(c2s_buf + c2s_len, 1, header_buf, header_len);
    /* Field 4: refreshCache (optional) */
    if (refresh_cache) {
        c2s_len += encode_bool(c2s_buf + c2s_len, 4, 1);
    }

    /* Build Request wrapper */
    size_t req_len = 0;
    req_len += encode_message(buf + req_len, 1, c2s_buf, c2s_len);

    return req_len;
}

/* Build Trd_GetMaxTrdQtys request */
size_t build_get_max_trd_qtys(uint8_t *buf, uint64_t acc_id, int32_t trd_market,
                             int32_t order_type, const char *code, double price) {
    /* Build Header (TrdHeader) */
    uint8_t header_buf[128];
    size_t header_len = 0;

    header_len += encode_int32(header_buf + header_len, 1, TRD_ENV_REAL);
    header_len += encode_uint64(header_buf + header_len, 2, acc_id);
    header_len += encode_int32(header_buf + header_len, 3, trd_market);

    /* Build C2S message */
    uint8_t c2s_buf[512];
    size_t c2s_len = 0;

    /* Field 1: header */
    c2s_len += encode_message(c2s_buf + c2s_len, 1, header_buf, header_len);
    /* Field 2: orderType */
    c2s_len += encode_int32(c2s_buf + c2s_len, 2, order_type);
    /* Field 3: code */
    c2s_len += encode_string(c2s_buf + c2s_len, 3, code);
    /* Field 4: price */
    c2s_len += encode_field_key(c2s_buf + c2s_len, 4, 1);  /* wire_type=1 for double */
    memcpy(c2s_buf + c2s_len, &price, 8);
    c2s_len += 8;

    /* Build Request wrapper */
    size_t req_len = 0;
    req_len += encode_message(buf + req_len, 1, c2s_buf, c2s_len);

    return req_len;
}
