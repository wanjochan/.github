/*
 * mod_querystring - Query string parsing and encoding
 *
 * v2 System Layer API replacements:
 * - All libc calls replaced with shim_* API
 * - Memory management via shim_malloc/shim_free/shim_calloc
 * - String operations via shim_strlen/shim_strchr/shim_strdup
 */

#include "mod_querystring.h"
#include "../cosmorun_system/libc_shim/sys_libc_shim.h"
#include <ctype.h>

/* Helper: check if character needs URL encoding */
static int needs_encoding(char c) {
    if (isalnum(c)) return 0;
    if (c == '-' || c == '_' || c == '.' || c == '~') return 0;
    return 1;
}

/* Helper: convert hex digit to integer */
static int hex_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

/* Helper: convert integer to hex digit */
static char int_to_hex(int n) {
    static const char hex[] = "0123456789ABCDEF";
    return hex[n & 0xF];
}

/* URL encode a string */
char* qs_encode(const char* str) {
    if (!str) return NULL;

    // 使用 shim_strlen 替代 strlen
    size_t len = shim_strlen(str);
    size_t encoded_len = 0;

    /* Calculate required length */
    for (size_t i = 0; i < len; i++) {
        encoded_len += needs_encoding(str[i]) ? 3 : 1;
    }

    // 使用 shim_malloc 替代 malloc
    char* encoded = shim_malloc(encoded_len + 1);
    if (!encoded) return NULL;

    size_t pos = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)str[i];
        if (needs_encoding(c)) {
            encoded[pos++] = '%';
            encoded[pos++] = int_to_hex(c >> 4);
            encoded[pos++] = int_to_hex(c & 0xF);
        } else {
            encoded[pos++] = c;
        }
    }
    encoded[pos] = '\0';

    return encoded;
}

/* URL decode a string */
char* qs_decode(const char* str) {
    if (!str) return NULL;

    // 使用 shim_strlen 替代 strlen
    size_t len = shim_strlen(str);
    // 使用 shim_malloc 替代 malloc
    char* decoded = shim_malloc(len + 1);
    if (!decoded) return NULL;

    size_t pos = 0;
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '%') {
            /* Check if we have enough characters for a complete encoding */
            if (i + 2 >= len) {
                /* Incomplete encoding */
                // 使用 shim_free 替代 free
                shim_free(decoded);
                return NULL;
            }
            int high = hex_to_int(str[i + 1]);
            int low = hex_to_int(str[i + 2]);
            if (high >= 0 && low >= 0) {
                decoded[pos++] = (char)((high << 4) | low);
                i += 2;
            } else {
                /* Invalid encoding */
                // 使用 shim_free 替代 free
                shim_free(decoded);
                return NULL;
            }
        } else if (str[i] == '+') {
            /* + is space in query strings */
            decoded[pos++] = ' ';
        } else {
            decoded[pos++] = str[i];
        }
    }
    decoded[pos] = '\0';

    return decoded;
}

/* Parse query string with custom separators */
std_hashmap_t* qs_parse_custom(const char* query, char sep, char eq) {
    if (!query) return NULL;

    std_hashmap_t* map = std_hashmap_new();
    if (!map) return NULL;

    /* Handle empty query string */
    if (*query == '\0') return map;

    /* Create a working copy */
    // 使用 shim_strdup 替代 strdup
    char* query_copy = shim_strdup(query);
    if (!query_copy) {
        std_hashmap_free(map);
        return NULL;
    }

    char* pair = query_copy;
    char* next_pair;

    while (pair) {
        /* Find next separator */
        // 使用 shim_strchr 替代 strchr
        next_pair = shim_strchr(pair, sep);
        if (next_pair) {
            *next_pair = '\0';
            next_pair++;
        }

        /* Skip empty pairs */
        if (*pair == '\0') {
            pair = next_pair;
            continue;
        }

        /* Find equal sign */
        // 使用 shim_strchr 替代 strchr
        char* eq_pos = shim_strchr(pair, eq);
        char* key = pair;
        char* value = "";

        if (eq_pos) {
            *eq_pos = '\0';
            value = eq_pos + 1;
        } else {
            /* No value, key only */
            value = "";
        }

        /* Decode key and value */
        char* decoded_key = qs_decode(key);
        char* decoded_value = qs_decode(value);

        if (decoded_key && decoded_value) {
            /* Store in hashmap (shim_strdup for ownership) */
            // 使用 shim_strdup 替代 strdup
            std_hashmap_set(map, decoded_key, shim_strdup(decoded_value));
        }

        // 使用 shim_free 替代 free
        shim_free(decoded_key);
        shim_free(decoded_value);

        pair = next_pair;
    }

    // 使用 shim_free 替代 free
    shim_free(query_copy);
    return map;
}

/* Parse query string with default separators */
std_hashmap_t* qs_parse(const char* query_string) {
    return qs_parse_custom(query_string, '&', '=');
}

/* Helper context for stringification */
typedef struct {
    std_string_t* result;
    char sep;
    char eq;
    int first;
} stringify_ctx_t;

/* Iterator callback for stringification */
static void stringify_iter(const char* key, void* value, void* userdata) {
    stringify_ctx_t* ctx = (stringify_ctx_t*)userdata;
    const char* val = (const char*)value;

    if (!ctx->first) {
        std_string_append_char(ctx->result, ctx->sep);
    }
    ctx->first = 0;

    /* Encode key and value */
    char* encoded_key = qs_encode(key);
    char* encoded_value = qs_encode(val);

    if (encoded_key && encoded_value) {
        std_string_append(ctx->result, encoded_key);
        std_string_append_char(ctx->result, ctx->eq);
        std_string_append(ctx->result, encoded_value);
    }

    // 使用 shim_free 替代 free
    shim_free(encoded_key);
    shim_free(encoded_value);
}

/* Stringify hashmap with custom separators */
char* qs_stringify_custom(std_hashmap_t* params, char sep, char eq) {
    if (!params) return NULL;

    std_string_t* result = std_string_new("");
    if (!result) return NULL;

    stringify_ctx_t ctx = {
        .result = result,
        .sep = sep,
        .eq = eq,
        .first = 1
    };

    std_hashmap_foreach(params, stringify_iter, &ctx);

    /* Extract final string */
    const char* cstr = std_string_cstr(result);
    // 使用 shim_strdup 替代 strdup
    char* final = shim_strdup(cstr ? cstr : "");

    std_string_free(result);
    return final;
}

/* Stringify hashmap with default separators */
char* qs_stringify(std_hashmap_t* params) {
    return qs_stringify_custom(params, '&', '=');
}
