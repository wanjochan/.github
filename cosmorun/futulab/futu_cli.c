/*
 * Futu OpenD CLI - Command Line Interface (Refactored)
 *
 * Features:
 * - Initialize connection (InitConnect)
 * - Get user info (GetUserInfo)
 * - Get global state (GetGlobalState)
 * - Keep-alive (KeepAlive)
 * - Get real-time quotes (Qot_GetBasicQot)
 *
 * Build: ../cosmorun.exe futu_cli.c futu_utils.c
 * Usage: ./a.out [command] [args...]
 *   Commands:
 *     init              - Initialize connection
 *     userinfo          - Get user information
 *     state             - Get global state
 *     keepalive         - Send keep-alive
 *     quote <m> <code>  - Get real-time quote (m=market: 1=HK, 11=US, 21=SH)
 *     all               - Run all basic commands
 */

#include "futu_utils.h"
extern int atoi(const char *str);
extern uint64_t strtoull(const char *str, char **endptr, int base);
extern double atof(const char *str);

/* Forward declarations */
static int cmd_subscribe(int sock, int32_t market, const char *code);

/* ========== Command Implementations ========== */

/* Command: InitConnect */
static int cmd_init_connect(int sock) {
    printf("=== InitConnect ===\n");

    /* Build InitConnect.C2S message - match C++ SDK (only 3 fields) */
    uint8_t c2s_buf[256];
    size_t c2s_len = 0;

    c2s_len += encode_int32(c2s_buf + c2s_len, 1, 100);  /* clientVer = 100 */
    c2s_len += encode_string(c2s_buf + c2s_len, 2, "demo");  /* clientID = "demo" */
    c2s_len += encode_bool(c2s_buf + c2s_len, 3, 1);  /* recvNotify = true */

    /* Build Request wrapper */
    uint8_t req_buf[512];
    size_t req_len = 0;
    req_len += encode_message(req_buf + req_len, 1, c2s_buf, c2s_len);  /* c2s field */

    /* Send request */
    uint8_t resp_buf[2048];
    int resp_len = send_request(sock, FUTU_PROTO_ID_INIT_CONNECT, req_buf, req_len,
                                resp_buf, sizeof(resp_buf));
    if (resp_len < 0) {
        fprintf(stderr, "Failed to send InitConnect request\n");
        return -1;
    }

    /* Parse response */
    int32_t ret_type;
    const uint8_t *s2c_data;
    size_t s2c_len;

    if (parse_response_header(resp_buf, resp_len, &ret_type, &s2c_data, &s2c_len) < 0) {
        fprintf(stderr, "Failed to parse response\n");
        return -1;
    }

    printf("  retType: %d %s\n", ret_type, ret_type == 0 ? "(Success)" : "(Failed)");

    if (ret_type == 0 && s2c_data) {
        /* Parse InitConnect.S2C */
        size_t pos = 0;
        while (pos < s2c_len) {
            uint32_t field_number, wire_type;
            pos += decode_field_key(s2c_data + pos, &field_number, &wire_type);

            if (field_number == 1) {  /* serverVer */
                uint64_t val;
                pos += decode_varint(s2c_data + pos, &val);
                printf("  serverVer: %llu\n", (unsigned long long)val);
            } else if (field_number == 2) {  /* loginUserID */
                uint64_t val;
                pos += decode_varint(s2c_data + pos, &val);
                printf("  loginUserID: %llu\n", (unsigned long long)val);
            } else if (field_number == 3) {  /* connID */
                uint64_t val;
                pos += decode_varint(s2c_data + pos, &val);
                printf("  connID: %llu\n", (unsigned long long)val);
            } else if (field_number == 5) {  /* keepAliveInterval */
                uint64_t val;
                pos += decode_varint(s2c_data + pos, &val);
                printf("  keepAliveInterval: %llu seconds\n", (unsigned long long)val);
            } else {
                pos += skip_field(s2c_data + pos, wire_type);
            }
        }
    }

    printf("\n");
    return ret_type == 0 ? 0 : -1;
}

/* Command: GetUserInfo */
static int cmd_get_user_info(int sock) {
    printf("=== GetUserInfo ===\n");

    /* Build GetUserInfo.C2S message (empty) */
    uint8_t c2s_buf[64];
    size_t c2s_len = 0;

    /* Build Request wrapper */
    uint8_t req_buf[128];
    size_t req_len = 0;
    req_len += encode_message(req_buf + req_len, 1, c2s_buf, c2s_len);

    /* Send request */
    uint8_t resp_buf[4096];
    int resp_len = send_request(sock, FUTU_PROTO_ID_GET_USER_INFO, req_buf, req_len,
                                resp_buf, sizeof(resp_buf));
    if (resp_len < 0) {
        fprintf(stderr, "Failed to send GetUserInfo request\n");
        return -1;
    }

    /* Parse response */
    int32_t ret_type;
    const uint8_t *s2c_data;
    size_t s2c_len;

    if (parse_response_header(resp_buf, resp_len, &ret_type, &s2c_data, &s2c_len) < 0) {
        fprintf(stderr, "Failed to parse response\n");
        return -1;
    }

    printf("  retType: %d %s\n", ret_type, ret_type == 0 ? "(Success)" : "(Failed)");

    if (ret_type == 0 && s2c_data) {
        /* Parse GetUserInfo.S2C */
        size_t pos = 0;
        while (pos < s2c_len) {
            uint32_t field_number, wire_type;
            pos += decode_field_key(s2c_data + pos, &field_number, &wire_type);

            if (field_number == 1) {  /* nickName */
                uint64_t len;
                pos += decode_varint(s2c_data + pos, &len);
                printf("  nickName: %.*s\n", (int)len, s2c_data + pos);
                pos += len;
            } else if (field_number == 8) {  /* userID */
                uint64_t val;
                pos += decode_varint(s2c_data + pos, &val);
                printf("  userID: %lld\n", (long long)val);
            } else if (field_number == 4) {  /* hkQotRight */
                uint64_t val;
                pos += decode_varint(s2c_data + pos, &val);
                printf("  hkQotRight: %llu\n", (unsigned long long)val);
            } else if (field_number == 5) {  /* usQotRight */
                uint64_t val;
                pos += decode_varint(s2c_data + pos, &val);
                printf("  usQotRight: %llu\n", (unsigned long long)val);
            } else if (field_number == 14) {  /* subQuota */
                uint64_t val;
                pos += decode_varint(s2c_data + pos, &val);
                printf("  subQuota: %llu\n", (unsigned long long)val);
            } else {
                pos += skip_field(s2c_data + pos, wire_type);
            }
        }
    }

    printf("\n");
    return ret_type == 0 ? 0 : -1;
}

/* Command: GetGlobalState */
static int cmd_get_global_state(int sock) {
    printf("=== GetGlobalState ===\n");

    /* Build GetGlobalState.C2S message */
    uint8_t c2s_buf[64];
    size_t c2s_len = 0;
    c2s_len += encode_uint64(c2s_buf + c2s_len, 1, 0);  /* userID = 0 (deprecated) */

    /* Build Request wrapper */
    uint8_t req_buf[128];
    size_t req_len = 0;
    req_len += encode_message(req_buf + req_len, 1, c2s_buf, c2s_len);

    /* Send request */
    uint8_t resp_buf[4096];
    int resp_len = send_request(sock, FUTU_PROTO_ID_GET_GLOBAL_STATE, req_buf, req_len,
                                resp_buf, sizeof(resp_buf));
    if (resp_len < 0) {
        fprintf(stderr, "Failed to send GetGlobalState request\n");
        return -1;
    }

    /* Parse response */
    int32_t ret_type;
    const uint8_t *s2c_data;
    size_t s2c_len;

    if (parse_response_header(resp_buf, resp_len, &ret_type, &s2c_data, &s2c_len) < 0) {
        fprintf(stderr, "Failed to parse response\n");
        return -1;
    }

    printf("  retType: %d %s\n", ret_type, ret_type == 0 ? "(Success)" : "(Failed)");

    if (ret_type == 0 && s2c_data) {
        /* Parse GetGlobalState.S2C */
        size_t pos = 0;
        while (pos < s2c_len) {
            uint32_t field_number, wire_type;
            pos += decode_field_key(s2c_data + pos, &field_number, &wire_type);

            if (field_number == 1) {  /* marketHK */
                uint64_t val;
                pos += decode_varint(s2c_data + pos, &val);
                printf("  marketHK: %llu\n", (unsigned long long)val);
            } else if (field_number == 2) {  /* marketUS */
                uint64_t val;
                pos += decode_varint(s2c_data + pos, &val);
                printf("  marketUS: %llu\n", (unsigned long long)val);
            } else if (field_number == 6) {  /* qotLogined */
                uint64_t val;
                pos += decode_varint(s2c_data + pos, &val);
                printf("  qotLogined: %s\n", val ? "true" : "false");
            } else if (field_number == 7) {  /* trdLogined */
                uint64_t val;
                pos += decode_varint(s2c_data + pos, &val);
                printf("  trdLogined: %s\n", val ? "true" : "false");
            } else if (field_number == 8) {  /* serverVer */
                uint64_t val;
                pos += decode_varint(s2c_data + pos, &val);
                printf("  serverVer: %llu\n", (unsigned long long)val);
            } else if (field_number == 10) {  /* time */
                uint64_t val;
                pos += decode_varint(s2c_data + pos, &val);
                printf("  serverTime: %lld\n", (long long)val);
            } else {
                pos += skip_field(s2c_data + pos, wire_type);
            }
        }
    }

    printf("\n");
    return ret_type == 0 ? 0 : -1;
}

/* Command: KeepAlive */
static int cmd_keep_alive(int sock) {
    printf("=== KeepAlive ===\n");

    /* Build KeepAlive.C2S message */
    uint8_t c2s_buf[64];
    size_t c2s_len = 0;
    c2s_len += encode_uint64(c2s_buf + c2s_len, 1, (uint64_t)time(NULL));  /* time */

    /* Build Request wrapper */
    uint8_t req_buf[128];
    size_t req_len = 0;
    req_len += encode_message(req_buf + req_len, 1, c2s_buf, c2s_len);

    /* Send request */
    uint8_t resp_buf[2048];
    int resp_len = send_request(sock, FUTU_PROTO_ID_KEEP_ALIVE, req_buf, req_len,
                                resp_buf, sizeof(resp_buf));
    if (resp_len < 0) {
        fprintf(stderr, "Failed to send KeepAlive request\n");
        return -1;
    }

    /* Parse response */
    int32_t ret_type;
    const uint8_t *s2c_data;
    size_t s2c_len;

    if (parse_response_header(resp_buf, resp_len, &ret_type, &s2c_data, &s2c_len) < 0) {
        fprintf(stderr, "Failed to parse response\n");
        return -1;
    }

    printf("  retType: %d %s\n", ret_type, ret_type == 0 ? "(Success)" : "(Failed)");

    if (ret_type == 0 && s2c_data) {
        /* Parse KeepAlive.S2C */
        size_t pos = 0;
        while (pos < s2c_len) {
            uint32_t field_number, wire_type;
            pos += decode_field_key(s2c_data + pos, &field_number, &wire_type);

            if (field_number == 1) {  /* time */
                uint64_t val;
                pos += decode_varint(s2c_data + pos, &val);
                printf("  serverTime: %lld\n", (long long)val);
            } else {
                pos += skip_field(s2c_data + pos, wire_type);
            }
        }
    }

    printf("\n");
    return ret_type == 0 ? 0 : -1;
}

/* ========== Quote Commands ========== */

/* Command: Get K-Line Data */
static int cmd_get_kline(int sock, int32_t market, const char *code,
                        int32_t kl_type, int32_t rehab_type, int32_t max_count) {
    printf("=== Get K-Line: %d:%s (type=%d, rehab=%d, max=%d) ===\n",
           market, code, kl_type, rehab_type, max_count);

    /* Build request - use recent date range if not specified */
    /* For simplicity, request last 365 days by default */
    uint8_t req_buf[1024];
    size_t req_len = build_request_history_kl(req_buf, market, code, kl_type, rehab_type,
                                             "2024-01-01", "2025-12-31", max_count);

    /* Send request */
    uint8_t resp_buf[65536];
    int resp_len = send_request(sock, FUTU_PROTO_ID_QOT_REQUEST_HISTORY_KL, req_buf, req_len,
                                resp_buf, sizeof(resp_buf));
    if (resp_len < 0) {
        fprintf(stderr, "Failed to send RequestHistoryKL request\n");
        return -1;
    }

    /* Parse response */
    int32_t ret_type;
    const uint8_t *s2c_data;
    size_t s2c_len;

    if (parse_response_header(resp_buf, resp_len, &ret_type, &s2c_data, &s2c_len) < 0) {
        fprintf(stderr, "Failed to parse response\n");
        return -1;
    }

    printf("  retType: %d %s\n", ret_type, ret_type == 0 ? "(Success)" : "(Failed)");

    if (ret_type == 0 && s2c_data) {
        /* Parse K-line list (simplified - just count) */
        size_t pos = 0;
        int kl_count = 0;
        while (pos < s2c_len) {
            uint32_t field_number, wire_type;
            pos += decode_field_key(s2c_data + pos, &field_number, &wire_type);

            if (field_number == 3) {  /* klList */
                uint64_t len;
                pos += decode_varint(s2c_data + pos, &len);
                kl_count++;
                pos += len;
            } else {
                pos += skip_field(s2c_data + pos, wire_type);
            }
        }
        printf("  K-Line count: %d\n", kl_count);
    }

    printf("\n");
    return ret_type == 0 ? 0 : -1;
}

/* Command: Get Order Book */
static int cmd_get_order_book(int sock, int32_t market, const char *code, int32_t num) {
    printf("=== Get Order Book: %d:%s (levels=%d) ===\n", market, code, num);

    /* Subscribe to OrderBook first (required) - SubType_OrderBook = 2 */
    uint8_t security_buf[128];
    size_t security_len = encode_security(security_buf, market, code);

    uint8_t c2s_buf[512];
    size_t c2s_len = 0;
    c2s_len += encode_message(c2s_buf + c2s_len, 1, security_buf, security_len);
    c2s_len += encode_field_key(c2s_buf + c2s_len, 2, 0);
    c2s_len += encode_varint(c2s_buf + c2s_len, 2);  /* SubType_OrderBook = 2 */
    c2s_len += encode_bool(c2s_buf + c2s_len, 3, 1);

    uint8_t req_buf_sub[1024];
    size_t req_len_sub = 0;
    req_len_sub += encode_message(req_buf_sub + req_len_sub, 1, c2s_buf, c2s_len);

    uint8_t resp_buf_sub[2048];
    int resp_len_sub = send_request(sock, FUTU_PROTO_ID_QOT_SUB, req_buf_sub, req_len_sub,
                                    resp_buf_sub, sizeof(resp_buf_sub));
    if (resp_len_sub < 0) {
        fprintf(stderr, "Failed to subscribe OrderBook\n");
        return -1;
    }

    /* Build request */
    uint8_t req_buf[512];
    size_t req_len = build_get_order_book(req_buf, market, code, num);

    /* Send request */
    uint8_t resp_buf[16384];
    int resp_len = send_request(sock, FUTU_PROTO_ID_QOT_GET_ORDER_BOOK, req_buf, req_len,
                                resp_buf, sizeof(resp_buf));
    if (resp_len < 0) {
        fprintf(stderr, "Failed to send GetOrderBook request\n");
        return -1;
    }

    /* Parse response */
    int32_t ret_type;
    const uint8_t *s2c_data;
    size_t s2c_len;

    if (parse_response_header(resp_buf, resp_len, &ret_type, &s2c_data, &s2c_len) < 0) {
        fprintf(stderr, "Failed to parse response\n");
        return -1;
    }

    printf("  retType: %d %s\n", ret_type, ret_type == 0 ? "(Success)" : "(Failed)");

    if (ret_type == 0 && s2c_data) {
        printf("  Order book data received (%zu bytes)\n", s2c_len);
        /* Detailed parsing would require more code - showing basic info only */
    }

    printf("\n");
    return ret_type == 0 ? 0 : -1;
}

/* Parse BasicQot message and print quote info */
static void parse_basic_qot(const uint8_t *buf, size_t buf_len) {
    size_t pos = 0;
    int32_t market = 0;
    char code[32] = {0};
    char name[128] = {0};
    double cur_price = 0, high_price = 0, low_price = 0;
    double open_price = 0, last_close_price = 0, turnover = 0;
    int64_t volume = 0;

    while (pos < buf_len) {
        uint32_t field_number, wire_type;
        pos += decode_field_key(buf + pos, &field_number, &wire_type);

        if (field_number == 1) {  /* security */
            uint64_t sec_len;
            pos += decode_varint(buf + pos, &sec_len);
            const uint8_t *sec_buf = buf + pos;
            size_t sec_pos = 0;

            while (sec_pos < sec_len) {
                uint32_t sec_field, sec_wire;
                sec_pos += decode_field_key(sec_buf + sec_pos, &sec_field, &sec_wire);
                if (sec_field == 1) {
                    uint64_t val;
                    sec_pos += decode_varint(sec_buf + sec_pos, &val);
                    market = (int32_t)val;
                } else if (sec_field == 2) {
                    uint64_t len;
                    sec_pos += decode_varint(sec_buf + sec_pos, &len);
                    snprintf(code, sizeof(code), "%.*s", (int)len, sec_buf + sec_pos);
                    sec_pos += len;
                } else {
                    sec_pos += skip_field(sec_buf + sec_pos, sec_wire);
                }
            }
            pos += sec_len;
        } else if (field_number == 24) {  /* name */
            uint64_t len;
            pos += decode_varint(buf + pos, &len);
            snprintf(name, sizeof(name), "%.*s", (int)len, buf + pos);
            pos += len;
        } else if (field_number == 9 && wire_type == 1) {  /* curPrice */
            memcpy(&cur_price, buf + pos, 8);
            pos += 8;
        } else if (field_number == 6 && wire_type == 1) {  /* highPrice */
            memcpy(&high_price, buf + pos, 8);
            pos += 8;
        } else if (field_number == 8 && wire_type == 1) {  /* lowPrice */
            memcpy(&low_price, buf + pos, 8);
            pos += 8;
        } else if (field_number == 7 && wire_type == 1) {  /* openPrice */
            memcpy(&open_price, buf + pos, 8);
            pos += 8;
        } else if (field_number == 10 && wire_type == 1) {  /* lastClosePrice */
            memcpy(&last_close_price, buf + pos, 8);
            pos += 8;
        } else if (field_number == 11) {  /* volume */
            uint64_t val;
            pos += decode_varint(buf + pos, &val);
            volume = (int64_t)val;
        } else if (field_number == 12 && wire_type == 1) {  /* turnover */
            memcpy(&turnover, buf + pos, 8);
            pos += 8;
        } else {
            pos += skip_field(buf + pos, wire_type);
        }
    }

    /* Print quote information */
    printf("  Security: %s (%d)\n", code, market);
    if (name[0]) {
        printf("  Name: %s\n", name);
    }
    printf("  Current Price: %.3f\n", cur_price);
    printf("  Open: %.3f  High: %.3f  Low: %.3f\n", open_price, high_price, low_price);
    printf("  Last Close: %.3f\n", last_close_price);

    if (last_close_price > 0) {
        double change = cur_price - last_close_price;
        double change_pct = (change / last_close_price) * 100;
        printf("  Change: %+.3f (%+.2f%%)\n", change, change_pct);
    }

    printf("  Volume: %lld\n", (long long)volume);
    printf("  Turnover: %.2f\n", turnover);
}

/* Command: Subscribe to stock */
static int cmd_subscribe(int sock, int32_t market, const char *code) {
    uint8_t security_buf[128];
    size_t security_len = encode_security(security_buf, market, code);

    uint8_t c2s_buf[512];
    size_t c2s_len = 0;
    c2s_len += encode_message(c2s_buf + c2s_len, 1, security_buf, security_len);
    c2s_len += encode_field_key(c2s_buf + c2s_len, 2, 0);
    c2s_len += encode_varint(c2s_buf + c2s_len, 1);  /* SubType_Basic = 1 */
    c2s_len += encode_bool(c2s_buf + c2s_len, 3, 1);

    uint8_t req_buf[1024];
    size_t req_len = 0;
    req_len += encode_message(req_buf + req_len, 1, c2s_buf, c2s_len);

    uint8_t resp_buf[2048];
    int resp_len = send_request(sock, FUTU_PROTO_ID_QOT_SUB, req_buf, req_len,
                                resp_buf, sizeof(resp_buf));
    if (resp_len < 0) return -1;

    int32_t ret_type;
    const uint8_t *s2c_data;
    size_t s2c_len;
    parse_response_header(resp_buf, resp_len, &ret_type, &s2c_data, &s2c_len);
    return ret_type == 0 ? 0 : -1;
}

/* Command: Get Basic Quote */
static int cmd_get_quote(int sock, int32_t market, const char *code) {
    printf("=== Get Quote: %d:%s ===\n", market, code);

    /* Subscribe first */
    if (cmd_subscribe(sock, market, code) < 0) {
        fprintf(stderr, "Failed to subscribe\n");
        return -1;
    }

    /* Build request */
    uint8_t security_buf[128];
    size_t security_len = encode_security(security_buf, market, code);

    uint8_t c2s_buf[256];
    size_t c2s_len = 0;
    c2s_len += encode_message(c2s_buf + c2s_len, 1, security_buf, security_len);

    uint8_t req_buf[512];
    size_t req_len = 0;
    req_len += encode_message(req_buf + req_len, 1, c2s_buf, c2s_len);

    /* Send request */
    uint8_t resp_buf[8192];
    int resp_len = send_request(sock, FUTU_PROTO_ID_QOT_GET_BASIC_QOT, req_buf, req_len,
                                resp_buf, sizeof(resp_buf));
    if (resp_len < 0) {
        fprintf(stderr, "Failed to send GetBasicQot request\n");
        return -1;
    }

    /* Parse response */
    int32_t ret_type;
    const uint8_t *s2c_data;
    size_t s2c_len;

    if (parse_response_header(resp_buf, resp_len, &ret_type, &s2c_data, &s2c_len) < 0) {
        fprintf(stderr, "Failed to parse response\n");
        return -1;
    }

    printf("  retType: %d %s\n", ret_type, ret_type == 0 ? "(Success)" : "(Failed)");

    if (ret_type == 0 && s2c_data) {
        size_t pos = 0;
        while (pos < s2c_len) {
            uint32_t field_number, wire_type;
            pos += decode_field_key(s2c_data + pos, &field_number, &wire_type);

            if (field_number == 1) {  /* basicQotList */
                uint64_t len;
                pos += decode_varint(s2c_data + pos, &len);
                parse_basic_qot(s2c_data + pos, len);
                pos += len;
            } else {
                pos += skip_field(s2c_data + pos, wire_type);
            }
        }
    }

    printf("\n");
    return ret_type == 0 ? 0 : -1;
}

/* ========== Trade Commands ========== */

/* Command: Get Account List */
static int cmd_get_acc_list(int sock) {
    printf("=== Get Account List ===\n");

    /* Build request */
    uint8_t req_buf[256];
    size_t req_len = build_get_acc_list(req_buf, 0);

    /* Send request */
    uint8_t resp_buf[8192];
    int resp_len = send_request(sock, FUTU_PROTO_ID_TRD_GET_ACC_LIST, req_buf, req_len,
                                resp_buf, sizeof(resp_buf));
    if (resp_len < 0) {
        fprintf(stderr, "Failed to send GetAccList request\n");
        return -1;
    }

    /* Parse response */
    int32_t ret_type;
    const uint8_t *s2c_data;
    size_t s2c_len;

    if (parse_response_header(resp_buf, resp_len, &ret_type, &s2c_data, &s2c_len) < 0) {
        fprintf(stderr, "Failed to parse response\n");
        return -1;
    }

    printf("  retType: %d %s\n", ret_type, ret_type == 0 ? "(Success)" : "(Failed)");

    if (ret_type == 0 && s2c_data) {
        /* Parse account list (simplified) */
        printf("  Account list received (%zu bytes)\n", s2c_len);
        /* Detailed parsing would show accID, trdMarket, etc. */
    }

    printf("\n");
    return ret_type == 0 ? 0 : -1;
}

/* Command: Unlock Trade */
static int cmd_unlock_trade(int sock, const char *password) {
    printf("=== Unlock Trade ===\n");

    /* Build request */
    uint8_t req_buf[512];
    size_t req_len = build_unlock_trade(req_buf, password, 1);

    /* Send request */
    uint8_t resp_buf[2048];
    int resp_len = send_request(sock, FUTU_PROTO_ID_TRD_UNLOCK_TRADE, req_buf, req_len,
                                resp_buf, sizeof(resp_buf));
    if (resp_len < 0) {
        fprintf(stderr, "Failed to send UnlockTrade request\n");
        return -1;
    }

    /* Parse response */
    int32_t ret_type;
    const uint8_t *s2c_data;
    size_t s2c_len;

    if (parse_response_header(resp_buf, resp_len, &ret_type, &s2c_data, &s2c_len) < 0) {
        fprintf(stderr, "Failed to parse response\n");
        return -1;
    }

    printf("  retType: %d %s\n", ret_type, ret_type == 0 ? "(Success)" : "(Failed)");
    printf("\n");
    return ret_type == 0 ? 0 : -1;
}

/* Command: Get Funds */
static int cmd_get_funds(int sock, uint64_t acc_id, int32_t trd_market) {
    printf("=== Get Funds (acc_id=%llu, market=%d) ===\n",
           (unsigned long long)acc_id, trd_market);

    /* Build request */
    uint8_t req_buf[512];
    size_t req_len = build_get_funds(req_buf, acc_id, trd_market, 0);

    /* Send request */
    uint8_t resp_buf[8192];
    int resp_len = send_request(sock, FUTU_PROTO_ID_TRD_GET_FUNDS, req_buf, req_len,
                                resp_buf, sizeof(resp_buf));
    if (resp_len < 0) {
        fprintf(stderr, "Failed to send GetFunds request\n");
        return -1;
    }

    /* Parse response */
    int32_t ret_type;
    const uint8_t *s2c_data;
    size_t s2c_len;

    if (parse_response_header(resp_buf, resp_len, &ret_type, &s2c_data, &s2c_len) < 0) {
        fprintf(stderr, "Failed to parse response\n");
        return -1;
    }

    printf("  retType: %d %s\n", ret_type, ret_type == 0 ? "(Success)" : "(Failed)");

    if (ret_type == 0 && s2c_data) {
        printf("  Funds data received (%zu bytes)\n", s2c_len);
        /* Detailed parsing would show cash, power, market value, etc. */
    }

    printf("\n");
    return ret_type == 0 ? 0 : -1;
}

/* Command: Get Position List */
static int cmd_get_position(int sock, uint64_t acc_id, int32_t trd_market, const char *code) {
    printf("=== Get Position (acc_id=%llu, market=%d) ===\n",
           (unsigned long long)acc_id, trd_market);

    /* Build request */
    uint8_t req_buf[1024];
    size_t req_len = build_get_position_list(req_buf, acc_id, trd_market, code);

    /* Send request */
    uint8_t resp_buf[16384];
    int resp_len = send_request(sock, FUTU_PROTO_ID_TRD_GET_POSITION_LIST, req_buf, req_len,
                                resp_buf, sizeof(resp_buf));
    if (resp_len < 0) {
        fprintf(stderr, "Failed to send GetPositionList request\n");
        return -1;
    }

    /* Parse response */
    int32_t ret_type;
    const uint8_t *s2c_data;
    size_t s2c_len;

    if (parse_response_header(resp_buf, resp_len, &ret_type, &s2c_data, &s2c_len) < 0) {
        fprintf(stderr, "Failed to parse response\n");
        return -1;
    }

    printf("  retType: %d %s\n", ret_type, ret_type == 0 ? "(Success)" : "(Failed)");

    if (ret_type == 0 && s2c_data) {
        printf("  Position list received (%zu bytes)\n", s2c_len);
        /* Detailed parsing would show code, qty, cost, etc. */
    }

    printf("\n");
    return ret_type == 0 ? 0 : -1;
}

/* Command: Place Order (WARNING: Real trading - use with caution!) */
static int cmd_place_order(int sock, uint64_t acc_id, int32_t trd_market,
                          int32_t trd_side, int32_t order_type,
                          const char *code, double price, double qty) {
    printf("=== Place Order (REAL TRADING!) ===\n");
    printf("  Account: %llu\n", (unsigned long long)acc_id);
    printf("  Market: %d, Side: %d, Type: %d\n", trd_market, trd_side, order_type);
    printf("  Code: %s, Price: %.3f, Qty: %.0f\n", code, price, qty);

    /* Build request */
    uint8_t req_buf[1024];
    size_t req_len = build_place_order(req_buf, acc_id, trd_market, trd_side, order_type,
                                      code, price, qty);

    /* Send request */
    uint8_t resp_buf[8192];
    int resp_len = send_request(sock, FUTU_PROTO_ID_TRD_PLACE_ORDER, req_buf, req_len,
                                resp_buf, sizeof(resp_buf));
    if (resp_len < 0) {
        fprintf(stderr, "Failed to send PlaceOrder request\n");
        return -1;
    }

    /* Parse response */
    int32_t ret_type;
    const uint8_t *s2c_data;
    size_t s2c_len;

    if (parse_response_header(resp_buf, resp_len, &ret_type, &s2c_data, &s2c_len) < 0) {
        fprintf(stderr, "Failed to parse response\n");
        return -1;
    }

    printf("  retType: %d %s\n", ret_type, ret_type == 0 ? "(Success)" : "(Failed)");

    if (ret_type == 0 && s2c_data) {
        printf("  Order placed! Data: %zu bytes\n", s2c_len);
        /* Detailed parsing would show orderID, status, etc. */
    }

    printf("\n");
    return ret_type == 0 ? 0 : -1;
}

/* Command: Modify Order (cancel/modify) */
static int cmd_modify_order(int sock, uint64_t acc_id, int32_t trd_market,
                           uint64_t order_id, int32_t modify_op,
                           double price, double qty) {
    printf("=== Modify Order ===\n");
    printf("  OrderID: %llu, Operation: %d\n", (unsigned long long)order_id, modify_op);

    /* Build request */
    uint8_t req_buf[1024];
    size_t req_len = build_modify_order(req_buf, acc_id, trd_market, order_id, modify_op,
                                       price, qty);

    /* Send request */
    uint8_t resp_buf[8192];
    int resp_len = send_request(sock, FUTU_PROTO_ID_TRD_MODIFY_ORDER, req_buf, req_len,
                                resp_buf, sizeof(resp_buf));
    if (resp_len < 0) {
        fprintf(stderr, "Failed to send ModifyOrder request\n");
        return -1;
    }

    /* Parse response */
    int32_t ret_type;
    const uint8_t *s2c_data;
    size_t s2c_len;

    if (parse_response_header(resp_buf, resp_len, &ret_type, &s2c_data, &s2c_len) < 0) {
        fprintf(stderr, "Failed to parse response\n");
        return -1;
    }

    printf("  retType: %d %s\n", ret_type, ret_type == 0 ? "(Success)" : "(Failed)");

    if (ret_type == 0 && s2c_data) {
        printf("  Order modified! Data: %zu bytes\n", s2c_len);
    }

    printf("\n");
    return ret_type == 0 ? 0 : -1;
}

/* Command: Get Order List */
static int cmd_get_order_list(int sock, uint64_t acc_id, int32_t trd_market) {
    printf("=== Get Order List (acc_id=%llu, market=%d) ===\n",
           (unsigned long long)acc_id, trd_market);

    /* Build request */
    uint8_t req_buf[512];
    size_t req_len = build_get_order_list(req_buf, acc_id, trd_market, 1);

    /* Send request */
    uint8_t resp_buf[65536];
    int resp_len = send_request(sock, FUTU_PROTO_ID_TRD_GET_ORDER_LIST, req_buf, req_len,
                                resp_buf, sizeof(resp_buf));
    if (resp_len < 0) {
        fprintf(stderr, "Failed to send GetOrderList request\n");
        return -1;
    }

    /* Parse response */
    int32_t ret_type;
    const uint8_t *s2c_data;
    size_t s2c_len;

    if (parse_response_header(resp_buf, resp_len, &ret_type, &s2c_data, &s2c_len) < 0) {
        fprintf(stderr, "Failed to parse response\n");
        return -1;
    }

    printf("  retType: %d %s\n", ret_type, ret_type == 0 ? "(Success)" : "(Failed)");

    if (ret_type == 0 && s2c_data) {
        printf("  Order list received (%zu bytes)\n", s2c_len);
        /* Detailed parsing would show orderID, code, price, qty, status, etc. */
    }

    printf("\n");
    return ret_type == 0 ? 0 : -1;
}

/* Command: Get Max Trade Quantities */
static int cmd_get_max_trd_qtys(int sock, uint64_t acc_id, int32_t trd_market,
                               int32_t order_type, const char *code, double price) {
    printf("=== Get Max Trade Quantities ===\n");
    printf("  Code: %s, Order Type: %d, Price: %.3f\n", code, order_type, price);

    /* Build request */
    uint8_t req_buf[1024];
    size_t req_len = build_get_max_trd_qtys(req_buf, acc_id, trd_market, order_type, code, price);

    /* Send request */
    uint8_t resp_buf[8192];
    int resp_len = send_request(sock, FUTU_PROTO_ID_TRD_GET_MAX_TRD_QTYS, req_buf, req_len,
                                resp_buf, sizeof(resp_buf));
    if (resp_len < 0) {
        fprintf(stderr, "Failed to send GetMaxTrdQtys request\n");
        return -1;
    }

    /* Parse response */
    int32_t ret_type;
    const uint8_t *s2c_data;
    size_t s2c_len;

    if (parse_response_header(resp_buf, resp_len, &ret_type, &s2c_data, &s2c_len) < 0) {
        fprintf(stderr, "Failed to parse response\n");
        return -1;
    }

    printf("  retType: %d %s\n", ret_type, ret_type == 0 ? "(Success)" : "(Failed)");

    if (ret_type == 0 && s2c_data) {
        printf("  Max trade quantities received (%zu bytes)\n", s2c_len);
        /* Detailed parsing would show maxCashBuy, maxSellShort, etc. */
    }

    printf("\n");
    return ret_type == 0 ? 0 : -1;
}

/* ========== Main ========== */

static void print_usage(const char *prog) {
    printf("Usage: %s [command] [args...]\n", prog);
    printf("\nInit Commands:\n");
    printf("  init                  - Initialize connection\n");
    printf("  userinfo              - Get user information\n");
    printf("  state                 - Get global state\n");
    printf("  keepalive             - Send keep-alive\n");
    printf("  all                   - Run all init commands\n");
    printf("\nQuote Commands:\n");
    printf("  quote <m> <code>      - Get real-time quote\n");
    printf("  kline <m> <code> [type] [rehab] [max] - Get K-line data\n");
    printf("  orderbook <m> <code> [levels] - Get order book\n");
    printf("\nTrade Commands (REAL ACCOUNT!):\n");
    printf("  acclist               - Get account list\n");
    printf("  unlock <pwd_md5>      - Unlock trade\n");
    printf("  funds <acc_id> <market> - Get funds\n");
    printf("  position <acc_id> <market> [code] - Get position\n");
    printf("  orderlist <acc_id> <market> - Get order list\n");
    printf("  maxqty <acc_id> <market> <type> <code> <price> - Get max trade quantities\n");
    printf("  order <acc_id> <market> <side> <type> <code> <price> <qty> - Place order\n");
    printf("  cancel <acc_id> <market> <order_id> - Cancel order\n");
    printf("  modify <acc_id> <market> <order_id> <price> <qty> - Modify order\n");
    printf("\nParameters:\n");
    printf("  m (market): 1=HK, 11=US, 21=SH, 22=SZ\n");
    printf("  type (K-line): 1=1min, 2=day, 3=week, 4=month, 7=5min, 8=15min, 9=30min, 10=60min\n");
    printf("  rehab: 0=none, 1=forward, 2=backward\n");
    printf("  side: 1=buy, 2=sell\n");
    printf("  order_type: 0=normal, 1=market\n");
    printf("  modify_op: 1=cancel, 2=modify\n");
    printf("\nExamples:\n");
    printf("  %s quote 1 00700           # Real-time quote\n", prog);
    printf("  %s kline 1 00700 2 1 10    # Daily K-line, forward rehab, 10 bars\n", prog);
    printf("  %s orderbook 1 00700 10    # Order book 10 levels\n", prog);
    printf("  %s acclist                 # Get trading accounts\n", prog);
    printf("\n");
}

int main(int argc, char *argv[]) {
    const char *host = "127.0.0.1";
    int port = 11111;

    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *cmd = argv[1];

    /* Connect to OpenD */
    printf("Connecting to %s:%d...\n", host, port);
    int sock = connect_to_opend(host, port);
    if (sock < 0) {
        fprintf(stderr, "Failed to connect to OpenD\n");
        fprintf(stderr, "Make sure Futu OpenD is running on %s:%d\n", host, port);
        return 1;
    }
    printf("Connected!\n\n");

    /* Execute command */
    int ret = 0;

    if (strcmp(cmd, "init") == 0) {
        ret = cmd_init_connect(sock);
    } else if (strcmp(cmd, "all") == 0) {
        cmd_init_connect(sock);
        cmd_get_user_info(sock);
        cmd_get_global_state(sock);
        cmd_keep_alive(sock);
    } else {
        /* Auto-init for all other commands */
        if (cmd_init_connect(sock) < 0) {
            fprintf(stderr, "Failed to initialize connection\n");
            close(sock);
            return 1;
        }

        if (strcmp(cmd, "userinfo") == 0) {
            ret = cmd_get_user_info(sock);
        } else if (strcmp(cmd, "state") == 0) {
            ret = cmd_get_global_state(sock);
        } else if (strcmp(cmd, "keepalive") == 0) {
            ret = cmd_keep_alive(sock);
        } else if (strcmp(cmd, "quote") == 0) {
            if (argc < 4) {
                fprintf(stderr, "Usage: %s quote <market> <code>\n", argv[0]);
                fprintf(stderr, "Example: %s quote 1 00700\n", argv[0]);
                close(sock);
                return 1;
            }
            int32_t market = atoi(argv[2]);
            const char *code = argv[3];
            ret = cmd_get_quote(sock, market, code);
        } else if (strcmp(cmd, "kline") == 0) {
            if (argc < 4) {
                fprintf(stderr, "Usage: %s kline <market> <code> [type] [rehab] [max]\n", argv[0]);
                close(sock);
                return 1;
            }
            int32_t market = atoi(argv[2]);
            const char *code = argv[3];
            int32_t kl_type = argc > 4 ? atoi(argv[4]) : KLINE_TYPE_DAY;
            int32_t rehab = argc > 5 ? atoi(argv[5]) : REHAB_TYPE_FORWARD;
            int32_t max_count = argc > 6 ? atoi(argv[6]) : 10;
            ret = cmd_get_kline(sock, market, code, kl_type, rehab, max_count);
        } else if (strcmp(cmd, "orderbook") == 0) {
            if (argc < 4) {
                fprintf(stderr, "Usage: %s orderbook <market> <code> [levels]\n", argv[0]);
                close(sock);
                return 1;
            }
            int32_t market = atoi(argv[2]);
            const char *code = argv[3];
            int32_t levels = argc > 4 ? atoi(argv[4]) : 10;
            ret = cmd_get_order_book(sock, market, code, levels);
        } else if (strcmp(cmd, "acclist") == 0) {
            ret = cmd_get_acc_list(sock);
        } else if (strcmp(cmd, "unlock") == 0) {
            if (argc < 3) {
                fprintf(stderr, "Usage: %s unlock <pwd_md5>\n", argv[0]);
                close(sock);
                return 1;
            }
            ret = cmd_unlock_trade(sock, argv[2]);
        } else if (strcmp(cmd, "funds") == 0) {
            if (argc < 4) {
                fprintf(stderr, "Usage: %s funds <acc_id> <market>\n", argv[0]);
                close(sock);
                return 1;
            }
            uint64_t acc_id = strtoull(argv[2], NULL, 10);
            int32_t market = atoi(argv[3]);
            ret = cmd_get_funds(sock, acc_id, market);
        } else if (strcmp(cmd, "position") == 0) {
            if (argc < 4) {
                fprintf(stderr, "Usage: %s position <acc_id> <market> [code]\n", argv[0]);
                close(sock);
                return 1;
            }
            uint64_t acc_id = strtoull(argv[2], NULL, 10);
            int32_t market = atoi(argv[3]);
            const char *code = argc > 4 ? argv[4] : "";
            ret = cmd_get_position(sock, acc_id, market, code);
        } else if (strcmp(cmd, "order") == 0) {
            if (argc < 9) {
                fprintf(stderr, "Usage: %s order <acc_id> <market> <side> <type> <code> <price> <qty>\n", argv[0]);
                fprintf(stderr, "WARNING: This will place a REAL order!\n");
                close(sock);
                return 1;
            }
            uint64_t acc_id = strtoull(argv[2], NULL, 10);
            int32_t market = atoi(argv[3]);
            int32_t side = atoi(argv[4]);
            int32_t order_type = atoi(argv[5]);
            const char *code = argv[6];
            double price = atof(argv[7]);
            double qty = atof(argv[8]);
            ret = cmd_place_order(sock, acc_id, market, side, order_type, code, price, qty);
        } else if (strcmp(cmd, "orderlist") == 0) {
            if (argc < 4) {
                fprintf(stderr, "Usage: %s orderlist <acc_id> <market>\n", argv[0]);
                close(sock);
                return 1;
            }
            uint64_t acc_id = strtoull(argv[2], NULL, 10);
            int32_t market = atoi(argv[3]);
            ret = cmd_get_order_list(sock, acc_id, market);
        } else if (strcmp(cmd, "maxqty") == 0) {
            if (argc < 7) {
                fprintf(stderr, "Usage: %s maxqty <acc_id> <market> <type> <code> <price>\n", argv[0]);
                close(sock);
                return 1;
            }
            uint64_t acc_id = strtoull(argv[2], NULL, 10);
            int32_t market = atoi(argv[3]);
            int32_t order_type = atoi(argv[4]);
            const char *code = argv[5];
            double price = atof(argv[6]);
            ret = cmd_get_max_trd_qtys(sock, acc_id, market, order_type, code, price);
        } else if (strcmp(cmd, "cancel") == 0) {
            if (argc < 5) {
                fprintf(stderr, "Usage: %s cancel <acc_id> <market> <order_id>\n", argv[0]);
                fprintf(stderr, "WARNING: This will CANCEL a REAL order!\n");
                close(sock);
                return 1;
            }
            uint64_t acc_id = strtoull(argv[2], NULL, 10);
            int32_t market = atoi(argv[3]);
            uint64_t order_id = strtoull(argv[4], NULL, 10);
            ret = cmd_modify_order(sock, acc_id, market, order_id, MODIFY_ORDER_OP_CANCEL, 0, 0);
        } else if (strcmp(cmd, "modify") == 0) {
            if (argc < 7) {
                fprintf(stderr, "Usage: %s modify <acc_id> <market> <order_id> <price> <qty>\n", argv[0]);
                fprintf(stderr, "WARNING: This will MODIFY a REAL order!\n");
                close(sock);
                return 1;
            }
            uint64_t acc_id = strtoull(argv[2], NULL, 10);
            int32_t market = atoi(argv[3]);
            uint64_t order_id = strtoull(argv[4], NULL, 10);
            double price = atof(argv[5]);
            double qty = atof(argv[6]);
            ret = cmd_modify_order(sock, acc_id, market, order_id, MODIFY_ORDER_OP_MODIFY, price, qty);
        } else {
            fprintf(stderr, "Unknown command: %s\n", cmd);
            print_usage(argv[0]);
            ret = 1;
        }
    }

    close(sock);
    return ret;
}
