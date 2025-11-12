#include "../cosmorun_system/libc_shim/sys_libc_shim.h"
/*
 * mod_buffer.c - Buffer implementation for cosmorun v2
 *
 * 本文件实现了缓冲区管理模块，提供：
 * - 动态内存管理 (使用 System Layer shim 层)
 * - 编码/解码支持 (UTF8, HEX, BASE64 等)
 * - 缓冲区操作 (复制、切片、填充)
 * - 比较和搜索功能
 */

#include "mod_buffer.h"
#include "mod_error_impl.h"

/* ==================== Base64 Encoding Tables ==================== */

static const char base64_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const unsigned char base64_decode_table[256] = {
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,62,64,64,64,63,
    52,53,54,55,56,57,58,59,60,61,64,64,64,64,64,64,
    64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
    15,16,17,18,19,20,21,22,23,24,25,64,64,64,64,64,
    64,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
    41,42,43,44,45,46,47,48,49,50,51,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,
    64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64
};

/* ==================== Helper Functions ==================== */

static void buffer_ensure_capacity(buffer_t *buf, size_t required) {
    if (!buf || buf->capacity >= required) return;

    size_t new_capacity = buf->capacity * 2;
    while (new_capacity < required) {
        new_capacity *= 2;
    }

    /* 使用 shim 层的 realloc 替代 libc */
    unsigned char *new_data = (unsigned char*)shim_realloc(buf->data, new_capacity);
    if (!new_data) return;

    buf->data = new_data;
    buf->capacity = new_capacity;
}

static int is_valid_utf8_byte(unsigned char c) {
    return (c & 0x80) == 0 || (c & 0xC0) == 0xC0;
}

static int hex_char_to_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static char value_to_hex_char(int val) {
    if (val < 10) return '0' + val;
    return 'a' + (val - 10);
}

/* ==================== Base64 Encoding/Decoding ==================== */

static size_t base64_encode_length(size_t input_len) {
    return ((input_len + 2) / 3) * 4;
}

static void base64_encode(const unsigned char *input, size_t input_len, char *output) {
    size_t i = 0, j = 0;

    while (input_len >= 3) {
        output[j++] = base64_chars[input[i] >> 2];
        output[j++] = base64_chars[((input[i] & 0x03) << 4) | (input[i+1] >> 4)];
        output[j++] = base64_chars[((input[i+1] & 0x0F) << 2) | (input[i+2] >> 6)];
        output[j++] = base64_chars[input[i+2] & 0x3F];
        i += 3;
        input_len -= 3;
    }

    if (input_len == 2) {
        output[j++] = base64_chars[input[i] >> 2];
        output[j++] = base64_chars[((input[i] & 0x03) << 4) | (input[i+1] >> 4)];
        output[j++] = base64_chars[(input[i+1] & 0x0F) << 2];
        output[j++] = '=';
    } else if (input_len == 1) {
        output[j++] = base64_chars[input[i] >> 2];
        output[j++] = base64_chars[(input[i] & 0x03) << 4];
        output[j++] = '=';
        output[j++] = '=';
    }

    output[j] = '\0';
}

static size_t base64_decode_length(const char *input, size_t input_len) {
    if (input_len == 0) return 0;

    size_t padding = 0;
    if (input[input_len - 1] == '=') padding++;
    if (input[input_len - 2] == '=') padding++;

    return (input_len / 4) * 3 - padding;
}

static int base64_decode(const char *input, size_t input_len, unsigned char *output) {
    if (input_len % 4 != 0) {
        COSMORUN_ERROR_RETURN(COSMORUN_ERR_INVALID_FORMAT, "Invalid base64 input length", -1);
    }

    size_t i = 0, j = 0;

    while (i < input_len) {
        unsigned char a = base64_decode_table[(unsigned char)input[i]];
        unsigned char b = base64_decode_table[(unsigned char)input[i+1]];
        unsigned char c = base64_decode_table[(unsigned char)input[i+2]];
        unsigned char d = base64_decode_table[(unsigned char)input[i+3]];

        if (a == 64 || b == 64) {
            COSMORUN_ERROR_RETURN(COSMORUN_ERR_INVALID_FORMAT, "Invalid base64 character", -1);
        }

        output[j++] = (a << 2) | (b >> 4);

        if (input[i+2] != '=') {
            if (c == 64) {
                COSMORUN_ERROR_RETURN(COSMORUN_ERR_INVALID_FORMAT, "Invalid base64 character", -1);
            }
            output[j++] = (b << 4) | (c >> 2);
        }

        if (input[i+3] != '=') {
            if (d == 64) {
                COSMORUN_ERROR_RETURN(COSMORUN_ERR_INVALID_FORMAT, "Invalid base64 character", -1);
            }
            output[j++] = (c << 6) | d;
        }

        i += 4;
    }

    return j;
}

/* ==================== Buffer Creation ==================== */

buffer_t* buffer_alloc(size_t size) {
    buffer_t *buf = (buffer_t*)shim_malloc(sizeof(buffer_t));
    if (!buf) {
        COSMORUN_ERROR_NULL(COSMORUN_ERR_OUT_OF_MEMORY, "Failed to allocate buffer");
    }

    buf->data = (unsigned char*)shim_calloc(size, 1);
    if (!buf->data) {
        shim_free(buf);
        COSMORUN_ERROR_NULL(COSMORUN_ERR_OUT_OF_MEMORY, "Failed to allocate buffer data");
    }

    buf->length = size;
    buf->capacity = size;

    return buf;
}

buffer_t* buffer_alloc_unsafe(size_t size) {
    buffer_t *buf = (buffer_t*)shim_malloc(sizeof(buffer_t));
    if (!buf) {
        COSMORUN_ERROR_NULL(COSMORUN_ERR_OUT_OF_MEMORY, "Failed to allocate buffer");
    }

    buf->data = (unsigned char*)shim_malloc(size);
    if (!buf->data) {
        shim_free(buf);
        COSMORUN_ERROR_NULL(COSMORUN_ERR_OUT_OF_MEMORY, "Failed to allocate buffer data");
    }

    buf->length = size;
    buf->capacity = size;

    return buf;
}

buffer_t* buffer_from_string(const char *str, buffer_encoding_t encoding) {
    if (!str) {
        COSMORUN_ERROR_NULL(COSMORUN_ERR_INVALID_ARG, "String is NULL");
    }

    size_t str_len = shim_strlen(str);
    buffer_t *buf = NULL;

    switch (encoding) {
        case BUFFER_ENCODING_UTF8:
        case BUFFER_ENCODING_ASCII:
        case BUFFER_ENCODING_BINARY:
            buf = buffer_alloc(str_len);
            if (buf) {
                shim_memcpy(buf->data, str, str_len);
            }
            break;

        case BUFFER_ENCODING_HEX: {
            if (str_len % 2 != 0) {
                COSMORUN_ERROR_NULL(COSMORUN_ERR_INVALID_FORMAT, "Invalid hex string length");
            }

            size_t byte_len = str_len / 2;
            buf = buffer_alloc(byte_len);
            if (!buf) {
                COSMORUN_ERROR_NULL(COSMORUN_ERR_OUT_OF_MEMORY, "Failed to allocate buffer");
            }

            for (size_t i = 0; i < byte_len; i++) {
                int high = hex_char_to_value(str[i * 2]);
                int low = hex_char_to_value(str[i * 2 + 1]);

                if (high < 0 || low < 0) {
                    buffer_free(buf);
                    COSMORUN_ERROR_NULL(COSMORUN_ERR_INVALID_FORMAT, "Invalid hex character");
                }

                buf->data[i] = (high << 4) | low;
            }
            break;
        }

        case BUFFER_ENCODING_BASE64: {
            size_t decoded_len = base64_decode_length(str, str_len);
            buf = buffer_alloc(decoded_len);
            if (!buf) {
                COSMORUN_ERROR_NULL(COSMORUN_ERR_OUT_OF_MEMORY, "Failed to allocate buffer");
            }

            int actual_len = base64_decode(str, str_len, buf->data);
            if (actual_len < 0) {
                buffer_free(buf);
                COSMORUN_ERROR_NULL(COSMORUN_ERR_INVALID_FORMAT, "Invalid base64 string");
            }

            buf->length = actual_len;
            break;
        }
    }

    return buf;
}

buffer_t* buffer_from_bytes(const unsigned char *data, size_t length) {
    if (!data) return NULL;

    buffer_t *buf = buffer_alloc(length);
    if (!buf) return NULL;

    shim_memcpy(buf->data, data, length);
    return buf;
}

buffer_t* buffer_concat(buffer_t **buffers, size_t count) {
    if (!buffers || count == 0) return NULL;

    size_t total_length = 0;
    for (size_t i = 0; i < count; i++) {
        if (buffers[i]) {
            total_length += buffers[i]->length;
        }
    }

    buffer_t *result = buffer_alloc(total_length);
    if (!result) return NULL;

    size_t offset = 0;
    for (size_t i = 0; i < count; i++) {
        if (buffers[i]) {
            shim_memcpy(result->data + offset, buffers[i]->data, buffers[i]->length);
            offset += buffers[i]->length;
        }
    }

    return result;
}

void buffer_free(buffer_t *buf) {
    if (!buf) return;
    if (buf->data) shim_free(buf->data);
    shim_free(buf);
}

/* ==================== String Conversion ==================== */

char* buffer_to_string(buffer_t *buf, buffer_encoding_t encoding) {
    if (!buf) return NULL;
    return buffer_to_string_range(buf, encoding, 0, buf->length);
}

char* buffer_to_string_range(buffer_t *buf, buffer_encoding_t encoding,
                              size_t start, size_t end) {
    if (!buf || start > end || end > buf->length) return NULL;

    size_t range_len = end - start;
    char *result = NULL;

    switch (encoding) {
        case BUFFER_ENCODING_UTF8:
        case BUFFER_ENCODING_ASCII:
        case BUFFER_ENCODING_BINARY:
            result = (char*)shim_malloc(range_len + 1);
            if (result) {
                shim_memcpy(result, buf->data + start, range_len);
                result[range_len] = '\0';
            }
            break;

        case BUFFER_ENCODING_HEX: {
            result = (char*)shim_malloc(range_len * 2 + 1);
            if (!result) return NULL;

            for (size_t i = 0; i < range_len; i++) {
                unsigned char byte = buf->data[start + i];
                result[i * 2] = value_to_hex_char(byte >> 4);
                result[i * 2 + 1] = value_to_hex_char(byte & 0x0F);
            }
            result[range_len * 2] = '\0';
            break;
        }

        case BUFFER_ENCODING_BASE64: {
            size_t encoded_len = base64_encode_length(range_len);
            result = (char*)shim_malloc(encoded_len + 1);
            if (!result) return NULL;

            base64_encode(buf->data + start, range_len, result);
            break;
        }
    }

    return result;
}

int buffer_write(buffer_t *buf, const char *str, size_t offset,
                 buffer_encoding_t encoding) {
    if (!buf || !str || offset >= buf->length) return -1;

    size_t str_len = shim_strlen(str);
    size_t available = buf->length - offset;

    switch (encoding) {
        case BUFFER_ENCODING_UTF8:
        case BUFFER_ENCODING_ASCII:
        case BUFFER_ENCODING_BINARY: {
            size_t write_len = (str_len < available) ? str_len : available;
            shim_memcpy(buf->data + offset, str, write_len);
            return write_len;
        }

        case BUFFER_ENCODING_HEX: {
            if (str_len % 2 != 0) return -1;

            size_t byte_len = str_len / 2;
            if (byte_len > available) byte_len = available;

            for (size_t i = 0; i < byte_len; i++) {
                int high = hex_char_to_value(str[i * 2]);
                int low = hex_char_to_value(str[i * 2 + 1]);

                if (high < 0 || low < 0) return -1;

                buf->data[offset + i] = (high << 4) | low;
            }

            return byte_len;
        }

        case BUFFER_ENCODING_BASE64: {
            size_t decoded_len = base64_decode_length(str, str_len);
            if (decoded_len > available) return -1;

            unsigned char *temp = (unsigned char*)shim_malloc(decoded_len);
            if (!temp) return -1;

            int actual_len = base64_decode(str, str_len, temp);
            if (actual_len < 0) {
                shim_free(temp);
                return -1;
            }

            shim_memcpy(buf->data + offset, temp, actual_len);
            shim_free(temp);

            return actual_len;
        }
    }

    return -1;
}

/* ==================== Buffer Manipulation ==================== */

int buffer_copy(buffer_t *source, buffer_t *target,
                size_t target_start, size_t source_start, size_t source_end) {
    if (!source || !target) return -1;
    if (source_start > source_end || source_end > source->length) return -1;
    if (target_start >= target->length) return -1;

    size_t copy_len = source_end - source_start;
    size_t available = target->length - target_start;

    if (copy_len > available) {
        copy_len = available;
    }

    shim_memmove(target->data + target_start, source->data + source_start, copy_len);
    return copy_len;
}

buffer_t* buffer_slice(buffer_t *buf, size_t start, size_t end) {
    if (!buf || start > end || end > buf->length) return NULL;

    size_t slice_len = end - start;
    buffer_t *result = buffer_alloc(slice_len);
    if (!result) return NULL;

    shim_memcpy(result->data, buf->data + start, slice_len);
    return result;
}

void buffer_fill(buffer_t *buf, unsigned char value, size_t start, size_t end) {
    if (!buf || start > end || end > buf->length) return;

    shim_memset(buf->data + start, value, end - start);
}

/* ==================== Comparison ==================== */

int buffer_equals(buffer_t *a, buffer_t *b) {
    if (!a || !b) return 0;
    if (a->length != b->length) return 0;

    return shim_memcmp(a->data, b->data, a->length) == 0;
}

int buffer_compare(buffer_t *a, buffer_t *b) {
    if (!a || !b) return 0;

    size_t min_len = (a->length < b->length) ? a->length : b->length;
    int cmp = shim_memcmp(a->data, b->data, min_len);

    if (cmp != 0) return (cmp < 0) ? -1 : 1;

    if (a->length < b->length) return -1;
    if (a->length > b->length) return 1;
    return 0;
}

/* ==================== Search ==================== */

int buffer_index_of(buffer_t *buf, const unsigned char *value, size_t value_len) {
    if (!buf || !value || value_len == 0 || value_len > buf->length) return -1;

    for (size_t i = 0; i <= buf->length - value_len; i++) {
        if (shim_memcmp(buf->data + i, value, value_len) == 0) {
            return i;
        }
    }

    return -1;
}

int buffer_last_index_of(buffer_t *buf, const unsigned char *value, size_t value_len) {
    if (!buf || !value || value_len == 0 || value_len > buf->length) return -1;

    for (int i = buf->length - value_len; i >= 0; i--) {
        if (shim_memcmp(buf->data + i, value, value_len) == 0) {
            return i;
        }
    }

    return -1;
}

int buffer_includes(buffer_t *buf, const unsigned char *value, size_t value_len) {
    return buffer_index_of(buf, value, value_len) >= 0;
}

/* ==================== Utility ==================== */

size_t buffer_length(buffer_t *buf) {
    return buf ? buf->length : 0;
}

int buffer_resize(buffer_t *buf, size_t new_size) {
    if (!buf) return -1;

    if (new_size <= buf->capacity) {
        buf->length = new_size;
        return 0;
    }

    unsigned char *new_data = (unsigned char*)shim_realloc(buf->data, new_size);
    if (!new_data) return -1;

    buf->data = new_data;
    buf->length = new_size;
    buf->capacity = new_size;

    return 0;
}

const char* buffer_encoding_name(buffer_encoding_t encoding) {
    switch (encoding) {
        case BUFFER_ENCODING_UTF8: return "utf8";
        case BUFFER_ENCODING_ASCII: return "ascii";
        case BUFFER_ENCODING_HEX: return "hex";
        case BUFFER_ENCODING_BASE64: return "base64";
        case BUFFER_ENCODING_BINARY: return "binary";
        default: return "unknown";
    }
}
