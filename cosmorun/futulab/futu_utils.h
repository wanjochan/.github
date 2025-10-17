/*
 * Futu OpenD API Utilities - Header File
 *
 * Provides common utilities for Futu OpenD protocol:
 * - Protobuf encoding/decoding
 * - SHA1 hashing
 * - Protocol header building
 * - Network communication
 */

#ifndef FUTU_UTILS_H
#define FUTU_UTILS_H

#include "../cosmo_libc.h"

/* ========== Constants ========== */

/* Futu protocol IDs */
#define FUTU_PROTO_ID_INIT_CONNECT      1001
#define FUTU_PROTO_ID_GET_GLOBAL_STATE  1002
#define FUTU_PROTO_ID_NOTIFY            1003
#define FUTU_PROTO_ID_KEEP_ALIVE        1004
#define FUTU_PROTO_ID_GET_USER_INFO     1005
#define FUTU_PROTO_ID_GET_DELAY_STATS   1006

/* Quote protocols (3000+) */
#define FUTU_PROTO_ID_QOT_SUB              3001
#define FUTU_PROTO_ID_QOT_REG_PUSH         3002
#define FUTU_PROTO_ID_QOT_GET_BASIC_QOT    3004
#define FUTU_PROTO_ID_QOT_GET_KL           3006
#define FUTU_PROTO_ID_QOT_GET_TICKER       3010
#define FUTU_PROTO_ID_QOT_GET_ORDER_BOOK   3012
#define FUTU_PROTO_ID_QOT_REQUEST_HISTORY_KL 3103

/* Trade protocols (2000+) */
#define FUTU_PROTO_ID_TRD_GET_ACC_LIST     2001
#define FUTU_PROTO_ID_TRD_UNLOCK_TRADE     2005
#define FUTU_PROTO_ID_TRD_GET_FUNDS        2101
#define FUTU_PROTO_ID_TRD_GET_POSITION_LIST 2102  /* Fixed: was 2201 */
#define FUTU_PROTO_ID_TRD_GET_MAX_TRD_QTYS 2111
#define FUTU_PROTO_ID_TRD_GET_ORDER_LIST   2201  /* Fixed: was 2211 */
#define FUTU_PROTO_ID_TRD_PLACE_ORDER      2202
#define FUTU_PROTO_ID_TRD_MODIFY_ORDER     2205
#define FUTU_PROTO_ID_TRD_GET_HISTORY_ORDER_LIST 2221

/* Protocol format types */
#define FUTU_PROTO_FMT_PROTOBUF         0
#define FUTU_PROTO_FMT_JSON             1

/* Protocol constants */
#define FUTU_HEADER_SIZE                44

/* Market codes */
#define FUTU_MARKET_HK    1   /* Hong Kong */
#define FUTU_MARKET_US    11  /* US */
#define FUTU_MARKET_SH    21  /* China Shanghai */
#define FUTU_MARKET_SZ    22  /* China Shenzhen */

/* KLine types */
#define KLINE_TYPE_1MIN   1
#define KLINE_TYPE_DAY    2
#define KLINE_TYPE_WEEK   3
#define KLINE_TYPE_MONTH  4
#define KLINE_TYPE_QUARTER 5
#define KLINE_TYPE_YEAR   6
#define KLINE_TYPE_5MIN   7
#define KLINE_TYPE_15MIN  8
#define KLINE_TYPE_30MIN  9
#define KLINE_TYPE_60MIN  10

/* Rehab types (复权) */
#define REHAB_TYPE_NONE    0  /* No adjustment */
#define REHAB_TYPE_FORWARD 1  /* Forward adjustment */
#define REHAB_TYPE_BACKWARD 2 /* Backward adjustment */

/* Trading environment */
#define TRD_ENV_REAL    0  /* Real trading */
#define TRD_ENV_SIMULATE 1 /* Simulated trading */

/* Trading side */
#define TRD_SIDE_NONE     0  /* Unknown */
#define TRD_SIDE_BUY      1  /* Buy */
#define TRD_SIDE_SELL     2  /* Sell */

/* Order type */
#define ORDER_TYPE_NORMAL           0  /* Normal order (limit) */
#define ORDER_TYPE_MARKET           1  /* Market order */
#define ORDER_TYPE_ABSOLUTE_LIMIT   5  /* Absolute limit order */
#define ORDER_TYPE_AUCTION          6  /* Auction order */
#define ORDER_TYPE_AUCTION_LIMIT    7  /* Auction limit order */
#define ORDER_TYPE_SPECIAL_LIMIT   10  /* Special limit order */

/* Trade market */
#define TRD_MARKET_HK         1  /* HK stock */
#define TRD_MARKET_US         2  /* US stock */
#define TRD_MARKET_CN         3  /* CN stock */
#define TRD_MARKET_HKCC       4  /* HK futures */
#define TRD_MARKET_FUTURES    5  /* Futures */

/* Modify order operations */
#define MODIFY_ORDER_OP_NONE      0  /* Unknown */
#define MODIFY_ORDER_OP_CANCEL    1  /* Cancel order */
#define MODIFY_ORDER_OP_MODIFY    2  /* Modify price/qty */
#define MODIFY_ORDER_OP_ENABLE    3  /* Enable order */
#define MODIFY_ORDER_OP_DISABLE   4  /* Disable order */

/* Order status */
#define ORDER_STATUS_NONE            0  /* Unknown */
#define ORDER_STATUS_UNSUBMITTED     1  /* Not submitted */
#define ORDER_STATUS_SUBMITTING      2  /* Submitting */
#define ORDER_STATUS_SUBMITTED       3  /* Submitted */
#define ORDER_STATUS_FILLED_PART     4  /* Partially filled */
#define ORDER_STATUS_FILLED_ALL      5  /* Fully filled */
#define ORDER_STATUS_CANCELLING_PART 6  /* Partially cancelled (in progress) */
#define ORDER_STATUS_CANCELLED_PART  7  /* Partially cancelled (done) */
#define ORDER_STATUS_CANCELLING_ALL  8  /* Fully cancelled (in progress) */
#define ORDER_STATUS_CANCELLED_ALL   9  /* Fully cancelled (done) */
#define ORDER_STATUS_FAILED         10  /* Failed */
#define ORDER_STATUS_DISABLED       11  /* Disabled */
#define ORDER_STATUS_DELETED        12  /* Deleted */

/* ========== Data Structures ========== */

/* Futu protocol header structure (44 bytes) */
typedef struct {
    uint8_t header_flag[2];     /* "FT" */
    uint8_t proto_id[4];        /* Protocol ID (little-endian) */
    uint8_t proto_fmt_type;     /* Format type (0=protobuf) */
    uint8_t proto_ver;          /* Protocol version */
    uint8_t serial_no[4];       /* Serial number (little-endian) */
    uint8_t body_len[4];        /* Body length (little-endian) */
    uint8_t body_sha1[20];      /* SHA1 hash of body */
    uint8_t reserved[8];        /* Reserved */
} futu_header_t;

/* ========== Protobuf Encoding Functions ========== */

/* Encode variable-length integer (varint) */
size_t encode_varint(uint8_t *buf, uint64_t value);

/* Encode field key (field_number << 3 | wire_type) */
size_t encode_field_key(uint8_t *buf, uint32_t field_number, uint32_t wire_type);

/* Encode int32 field */
size_t encode_int32(uint8_t *buf, uint32_t field_number, int32_t value);

/* Encode uint64 field */
size_t encode_uint64(uint8_t *buf, uint32_t field_number, uint64_t value);

/* Encode bool field */
size_t encode_bool(uint8_t *buf, uint32_t field_number, int value);

/* Encode string field */
size_t encode_string(uint8_t *buf, uint32_t field_number, const char *str);

/* Encode embedded message field */
size_t encode_message(uint8_t *buf, uint32_t field_number, const uint8_t *msg, size_t msg_len);

/* ========== Protobuf Decoding Functions ========== */

/* Decode varint */
size_t decode_varint(const uint8_t *buf, uint64_t *value);

/* Decode field key */
size_t decode_field_key(const uint8_t *buf, uint32_t *field_number, uint32_t *wire_type);

/* Skip field based on wire type */
size_t skip_field(const uint8_t *buf, uint32_t wire_type);

/* ========== SHA1 Functions ========== */

/* Calculate SHA1 hash of data */
void sha1_hash(const uint8_t *data, uint32_t len, uint8_t digest[20]);

/* ========== Futu Protocol Functions ========== */

/* Build Futu protocol header with SHA1 hash */
void build_futu_header(futu_header_t *header, uint32_t proto_id,
                       uint32_t serial_no, const uint8_t *body, uint32_t body_len);

/* Connect to Futu OpenD server */
int connect_to_opend(const char *host, int port);

/* Send request and receive response */
int send_request(int sock, uint32_t proto_id, const uint8_t *body, uint32_t body_len,
                uint8_t *resp_buf, size_t resp_buf_size);

/* Parse common Response wrapper and extract s2c data */
int parse_response_header(const uint8_t *buf, size_t buf_len,
                         int32_t *ret_type, const uint8_t **s2c_data, size_t *s2c_len);

/* ========== Security Encoding (for Quote APIs) ========== */

/* Encode Security message (market + code) */
size_t encode_security(uint8_t *buf, int32_t market, const char *code);

/* ========== Quote API Helper Functions ========== */

/* Build Qot_RequestHistoryKL request */
size_t build_request_history_kl(uint8_t *buf, int32_t market, const char *code,
                                int32_t kl_type, int32_t rehab_type,
                                const char *begin_time, const char *end_time,
                                int32_t max_count);

/* Build Qot_GetOrderBook request */
size_t build_get_order_book(uint8_t *buf, int32_t market, const char *code, int32_t num);

/* ========== Trade API Helper Functions ========== */

/* Build Trd_GetAccList request */
size_t build_get_acc_list(uint8_t *buf, uint64_t user_id);

/* Build Trd_UnlockTrade request */
size_t build_unlock_trade(uint8_t *buf, const char *password, int is_unlock);

/* Build Trd_GetFunds request */
size_t build_get_funds(uint8_t *buf, uint64_t acc_id, int32_t trd_market, int refresh_cache);

/* Build Trd_GetPositionList request */
size_t build_get_position_list(uint8_t *buf, uint64_t acc_id, int32_t trd_market, const char *code);

/* Build Trd_PlaceOrder request */
size_t build_place_order(uint8_t *buf, uint64_t acc_id, int32_t trd_market,
                        int32_t trd_side, int32_t order_type,
                        const char *code, double price, double qty);

/* Build Trd_ModifyOrder request */
size_t build_modify_order(uint8_t *buf, uint64_t acc_id, int32_t trd_market,
                         uint64_t order_id, int32_t modify_op,
                         double price, double qty);

/* Build Trd_GetOrderList request */
size_t build_get_order_list(uint8_t *buf, uint64_t acc_id, int32_t trd_market,
                           int refresh_cache);

/* Build Trd_GetMaxTrdQtys request */
size_t build_get_max_trd_qtys(uint8_t *buf, uint64_t acc_id, int32_t trd_market,
                             int32_t order_type, const char *code, double price);

/* ========== Utility Functions ========== */

/* Print hex dump of buffer (for debugging) */
void print_hex(const char *label, const uint8_t *data, size_t len);

#endif /* FUTU_UTILS_H */
