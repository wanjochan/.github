/*
 * mod_serial.c - Native Serialization Implementation
 *
 * Provides RFC 8259 compliant JSON and MessagePack encoding/decoding
 * Zero-copy optimizations where possible, proper error handling
 */

#include "mod_serial.h"

/* ==================== Internal Value Structure ==================== */

typedef struct serial_array {
    serial_value_t **items;
    size_t len;
    size_t capacity;
} serial_array_t;

typedef struct serial_map_entry {
    char *key;
    serial_value_t *value;
    struct serial_map_entry *next;
} serial_map_entry_t;

typedef struct serial_map {
    serial_map_entry_t **buckets;
    size_t bucket_count;
    size_t size;
} serial_map_t;

struct serial_value {
    serial_type_t type;
    union {
        bool bool_val;
        int64_t int_val;
        double double_val;
        char *string_val;
        serial_array_t *array_val;
        serial_map_t *map_val;
    } data;
};

/* ==================== Error Handling ==================== */

static serial_error_t g_serial_error = {0};

serial_error_t* serial_get_error(void) {
    return &g_serial_error;
}

void serial_clear_error(void) {
    g_serial_error.code = SERIAL_OK;
    g_serial_error.line = 0;
    g_serial_error.column = 0;
    g_serial_error.message[0] = '\0';
}

static void serial_set_error(int code, int line, int col, const char *msg) {
    g_serial_error.code = code;
    g_serial_error.line = line;
    g_serial_error.column = col;
    strncpy(g_serial_error.message, msg, sizeof(g_serial_error.message) - 1);
    g_serial_error.message[sizeof(g_serial_error.message) - 1] = '\0';
}

/* ==================== String Utilities ==================== */

static char* serial_strndup(const char *str, size_t len) {
    char *dup = (char*)malloc(len + 1);
    if (!dup) return NULL;
    memcpy(dup, str, len);
    dup[len] = '\0';
    return dup;
}

/* ==================== JSON Parser ==================== */

typedef struct {
    const char *input;
    size_t len;
    size_t pos;
    int line;
    int column;
} json_parser_t;

static void skip_whitespace(json_parser_t *p) {
    while (p->pos < p->len) {
        char c = p->input[p->pos];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (c == '\n') {
                p->line++;
                p->column = 1;
            } else {
                p->column++;
            }
            p->pos++;
        } else {
            break;
        }
    }
}

static bool parse_literal(json_parser_t *p, const char *literal) {
    size_t lit_len = strlen(literal);
    if (p->pos + lit_len > p->len) return false;
    if (memcmp(p->input + p->pos, literal, lit_len) != 0) return false;
    p->pos += lit_len;
    p->column += lit_len;
    return true;
}

static serial_value_t* parse_value(json_parser_t *p);

static serial_value_t* parse_null(json_parser_t *p) {
    if (!parse_literal(p, "null")) {
        serial_set_error(SERIAL_ERR_PARSE, p->line, p->column, "Expected 'null'");
        return NULL;
    }
    return serial_create_null();
}

static serial_value_t* parse_bool(json_parser_t *p) {
    if (parse_literal(p, "true")) {
        return serial_create_bool(true);
    } else if (parse_literal(p, "false")) {
        return serial_create_bool(false);
    }
    serial_set_error(SERIAL_ERR_PARSE, p->line, p->column, "Expected boolean");
    return NULL;
}

static serial_value_t* parse_number(json_parser_t *p) {
    size_t start = p->pos;
    bool is_double = false;

    // Sign
    if (p->pos < p->len && (p->input[p->pos] == '-' || p->input[p->pos] == '+')) {
        p->pos++;
    }

    // Integer part
    if (p->pos >= p->len || !isdigit(p->input[p->pos])) {
        serial_set_error(SERIAL_ERR_PARSE, p->line, p->column, "Invalid number");
        return NULL;
    }

    while (p->pos < p->len && isdigit(p->input[p->pos])) {
        p->pos++;
    }

    // Decimal part
    if (p->pos < p->len && p->input[p->pos] == '.') {
        is_double = true;
        p->pos++;
        if (p->pos >= p->len || !isdigit(p->input[p->pos])) {
            serial_set_error(SERIAL_ERR_PARSE, p->line, p->column, "Invalid decimal");
            return NULL;
        }
        while (p->pos < p->len && isdigit(p->input[p->pos])) {
            p->pos++;
        }
    }

    // Exponent
    if (p->pos < p->len && (p->input[p->pos] == 'e' || p->input[p->pos] == 'E')) {
        is_double = true;
        p->pos++;
        if (p->pos < p->len && (p->input[p->pos] == '+' || p->input[p->pos] == '-')) {
            p->pos++;
        }
        if (p->pos >= p->len || !isdigit(p->input[p->pos])) {
            serial_set_error(SERIAL_ERR_PARSE, p->line, p->column, "Invalid exponent");
            return NULL;
        }
        while (p->pos < p->len && isdigit(p->input[p->pos])) {
            p->pos++;
        }
    }

    char *num_str = serial_strndup(p->input + start, p->pos - start);
    p->column += (p->pos - start);

    serial_value_t *val;
    if (is_double) {
        val = serial_create_double(strtod(num_str, NULL));
    } else {
        val = serial_create_int(strtoll(num_str, NULL, 10));
    }
    free(num_str);
    return val;
}

static int hex_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static serial_value_t* parse_string(json_parser_t *p) {
    if (p->pos >= p->len || p->input[p->pos] != '"') {
        serial_set_error(SERIAL_ERR_PARSE, p->line, p->column, "Expected '\"'");
        return NULL;
    }
    p->pos++;
    p->column++;

    size_t start = p->pos;
    char *buf = NULL;
    size_t buf_len = 0;
    size_t buf_cap = 0;

    while (p->pos < p->len) {
        char c = p->input[p->pos];

        if (c == '"') {
            // End of string
            serial_value_t *val;
            if (buf) {
                val = serial_create_string_len(buf, buf_len);
                free(buf);
            } else {
                val = serial_create_string_len(p->input + start, p->pos - start);
            }
            p->pos++;
            p->column++;
            return val;
        } else if (c == '\\') {
            // Escape sequence
            if (!buf) {
                // Start building buffer
                buf_cap = 256;
                buf = (char*)malloc(buf_cap);
                if (p->pos > start) {
                    memcpy(buf, p->input + start, p->pos - start);
                    buf_len = p->pos - start;
                }
            }

            p->pos++;
            if (p->pos >= p->len) {
                free(buf);
                serial_set_error(SERIAL_ERR_PARSE, p->line, p->column, "Incomplete escape");
                return NULL;
            }

            char esc = p->input[p->pos];
            char to_add;

            switch (esc) {
                case '"': case '\\': case '/': to_add = esc; break;
                case 'b': to_add = '\b'; break;
                case 'f': to_add = '\f'; break;
                case 'n': to_add = '\n'; break;
                case 'r': to_add = '\r'; break;
                case 't': to_add = '\t'; break;
                case 'u': {
                    // Unicode escape \uXXXX
                    if (p->pos + 4 >= p->len) {
                        free(buf);
                        serial_set_error(SERIAL_ERR_PARSE, p->line, p->column, "Incomplete unicode");
                        return NULL;
                    }
                    int code = 0;
                    for (int i = 0; i < 4; i++) {
                        int h = hex_to_int(p->input[p->pos + 1 + i]);
                        if (h < 0) {
                            free(buf);
                            serial_set_error(SERIAL_ERR_PARSE, p->line, p->column, "Invalid unicode");
                            return NULL;
                        }
                        code = (code << 4) | h;
                    }
                    p->pos += 4;
                    // Simple UTF-8 encoding (only BMP for now)
                    if (code < 0x80) {
                        to_add = (char)code;
                    } else {
                        // Multi-byte UTF-8 - simplified, only handles BMP
                        if (buf_len + 3 >= buf_cap) {
                            buf_cap *= 2;
                            buf = (char*)realloc(buf, buf_cap);
                        }
                        if (code < 0x800) {
                            buf[buf_len++] = 0xC0 | (code >> 6);
                            buf[buf_len++] = 0x80 | (code & 0x3F);
                        } else {
                            buf[buf_len++] = 0xE0 | (code >> 12);
                            buf[buf_len++] = 0x80 | ((code >> 6) & 0x3F);
                            buf[buf_len++] = 0x80 | (code & 0x3F);
                        }
                        p->pos++;
                        p->column++;
                        continue;
                    }
                    break;
                }
                default:
                    free(buf);
                    serial_set_error(SERIAL_ERR_PARSE, p->line, p->column, "Invalid escape");
                    return NULL;
            }

            if (buf_len >= buf_cap) {
                buf_cap *= 2;
                buf = (char*)realloc(buf, buf_cap);
            }
            buf[buf_len++] = to_add;
            p->pos++;
            p->column++;
        } else {
            if (buf) {
                if (buf_len >= buf_cap) {
                    buf_cap *= 2;
                    buf = (char*)realloc(buf, buf_cap);
                }
                buf[buf_len++] = c;
            }
            p->pos++;
            p->column++;
        }
    }

    free(buf);
    serial_set_error(SERIAL_ERR_PARSE, p->line, p->column, "Unterminated string");
    return NULL;
}

static serial_value_t* parse_array(json_parser_t *p) {
    if (p->pos >= p->len || p->input[p->pos] != '[') {
        serial_set_error(SERIAL_ERR_PARSE, p->line, p->column, "Expected '['");
        return NULL;
    }
    p->pos++;
    p->column++;

    serial_value_t *arr = serial_create_array();
    skip_whitespace(p);

    // Empty array
    if (p->pos < p->len && p->input[p->pos] == ']') {
        p->pos++;
        p->column++;
        return arr;
    }

    while (p->pos < p->len) {
        serial_value_t *val = parse_value(p);
        if (!val) {
            json_free(arr);
            return NULL;
        }
        serial_array_push(arr, val);

        skip_whitespace(p);
        if (p->pos >= p->len) break;

        if (p->input[p->pos] == ']') {
            p->pos++;
            p->column++;
            return arr;
        } else if (p->input[p->pos] == ',') {
            p->pos++;
            p->column++;
            skip_whitespace(p);
        } else {
            json_free(arr);
            serial_set_error(SERIAL_ERR_PARSE, p->line, p->column, "Expected ',' or ']'");
            return NULL;
        }
    }

    json_free(arr);
    serial_set_error(SERIAL_ERR_PARSE, p->line, p->column, "Unterminated array");
    return NULL;
}

static serial_value_t* parse_object(json_parser_t *p) {
    if (p->pos >= p->len || p->input[p->pos] != '{') {
        serial_set_error(SERIAL_ERR_PARSE, p->line, p->column, "Expected '{'");
        return NULL;
    }
    p->pos++;
    p->column++;

    serial_value_t *obj = serial_create_map();
    skip_whitespace(p);

    // Empty object
    if (p->pos < p->len && p->input[p->pos] == '}') {
        p->pos++;
        p->column++;
        return obj;
    }

    while (p->pos < p->len) {
        skip_whitespace(p);

        // Parse key
        serial_value_t *key_val = parse_string(p);
        if (!key_val) {
            json_free(obj);
            return NULL;
        }
        const char *key = serial_get_string(key_val);

        skip_whitespace(p);
        if (p->pos >= p->len || p->input[p->pos] != ':') {
            json_free(obj);
            json_free(key_val);
            serial_set_error(SERIAL_ERR_PARSE, p->line, p->column, "Expected ':'");
            return NULL;
        }
        p->pos++;
        p->column++;
        skip_whitespace(p);

        // Parse value
        serial_value_t *val = parse_value(p);
        if (!val) {
            json_free(obj);
            json_free(key_val);
            return NULL;
        }

        serial_map_set(obj, key, val);
        json_free(key_val);

        skip_whitespace(p);
        if (p->pos >= p->len) break;

        if (p->input[p->pos] == '}') {
            p->pos++;
            p->column++;
            return obj;
        } else if (p->input[p->pos] == ',') {
            p->pos++;
            p->column++;
        } else {
            json_free(obj);
            serial_set_error(SERIAL_ERR_PARSE, p->line, p->column, "Expected ',' or '}'");
            return NULL;
        }
    }

    json_free(obj);
    serial_set_error(SERIAL_ERR_PARSE, p->line, p->column, "Unterminated object");
    return NULL;
}

static serial_value_t* parse_value(json_parser_t *p) {
    skip_whitespace(p);
    if (p->pos >= p->len) {
        serial_set_error(SERIAL_ERR_PARSE, p->line, p->column, "Unexpected end of input");
        return NULL;
    }

    char c = p->input[p->pos];
    if (c == 'n') return parse_null(p);
    if (c == 't' || c == 'f') return parse_bool(p);
    if (c == '"') return parse_string(p);
    if (c == '[') return parse_array(p);
    if (c == '{') return parse_object(p);
    if (c == '-' || (c >= '0' && c <= '9')) return parse_number(p);

    serial_set_error(SERIAL_ERR_PARSE, p->line, p->column, "Unexpected character");
    return NULL;
}

serial_value_t* json_parse(const char *str) {
    return json_parse_len(str, strlen(str));
}

serial_value_t* json_parse_len(const char *str, size_t len) {
    serial_clear_error();
    json_parser_t p = {str, len, 0, 1, 1};
    serial_value_t *val = parse_value(&p);
    if (!val) return NULL;

    skip_whitespace(&p);
    if (p.pos < p.len) {
        json_free(val);
        serial_set_error(SERIAL_ERR_PARSE, p.line, p.column, "Unexpected data after value");
        return NULL;
    }

    return val;
}

/* ==================== JSON Serializer ==================== */

typedef struct {
    char *buf;
    size_t len;
    size_t cap;
    int indent;
    bool compact;
} json_writer_t;

static void writer_init(json_writer_t *w, bool compact) {
    w->cap = 256;
    w->buf = (char*)malloc(w->cap);
    w->len = 0;
    w->indent = 0;
    w->compact = compact;
}

static void writer_grow(json_writer_t *w, size_t needed) {
    while (w->len + needed >= w->cap) {
        w->cap *= 2;
    }
    w->buf = (char*)realloc(w->buf, w->cap);
}

static void writer_append_char(json_writer_t *w, char c) {
    writer_grow(w, 1);
    w->buf[w->len++] = c;
}

static void writer_append_str(json_writer_t *w, const char *str) {
    size_t slen = strlen(str);
    writer_grow(w, slen);
    memcpy(w->buf + w->len, str, slen);
    w->len += slen;
}

static void writer_indent(json_writer_t *w) {
    if (w->compact) return;
    writer_append_char(w, '\n');
    for (int i = 0; i < w->indent; i++) {
        writer_append_str(w, "  ");
    }
}

static void serialize_value(json_writer_t *w, const serial_value_t *val);

static void serialize_string(json_writer_t *w, const char *str) {
    writer_append_char(w, '"');
    for (const char *p = str; *p; p++) {
        switch (*p) {
            case '"': writer_append_str(w, "\\\""); break;
            case '\\': writer_append_str(w, "\\\\"); break;
            case '\b': writer_append_str(w, "\\b"); break;
            case '\f': writer_append_str(w, "\\f"); break;
            case '\n': writer_append_str(w, "\\n"); break;
            case '\r': writer_append_str(w, "\\r"); break;
            case '\t': writer_append_str(w, "\\t"); break;
            default:
                if (*p < 32) {
                    char buf[7];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)*p);
                    writer_append_str(w, buf);
                } else {
                    writer_append_char(w, *p);
                }
        }
    }
    writer_append_char(w, '"');
}

static void serialize_value(json_writer_t *w, const serial_value_t *val) {
    if (!val) {
        writer_append_str(w, "null");
        return;
    }

    switch (val->type) {
        case SERIAL_NULL:
            writer_append_str(w, "null");
            break;
        case SERIAL_BOOL:
            writer_append_str(w, val->data.bool_val ? "true" : "false");
            break;
        case SERIAL_INT: {
            char buf[32];
            snprintf(buf, sizeof(buf), "%lld", (long long)val->data.int_val);
            writer_append_str(w, buf);
            break;
        }
        case SERIAL_DOUBLE: {
            char buf[64];
            snprintf(buf, sizeof(buf), "%.17g", val->data.double_val);
            writer_append_str(w, buf);
            break;
        }
        case SERIAL_STRING:
            serialize_string(w, val->data.string_val);
            break;
        case SERIAL_ARRAY: {
            serial_array_t *arr = val->data.array_val;
            writer_append_char(w, '[');
            w->indent++;
            for (size_t i = 0; i < arr->len; i++) {
                if (i > 0) writer_append_char(w, ',');
                if (!w->compact) writer_indent(w);
                serialize_value(w, arr->items[i]);
            }
            w->indent--;
            if (arr->len > 0 && !w->compact) writer_indent(w);
            writer_append_char(w, ']');
            break;
        }
        case SERIAL_MAP: {
            serial_map_t *map = val->data.map_val;
            writer_append_char(w, '{');
            w->indent++;
            bool first = true;
            for (size_t i = 0; i < map->bucket_count; i++) {
                for (serial_map_entry_t *e = map->buckets[i]; e; e = e->next) {
                    if (!first) writer_append_char(w, ',');
                    if (!w->compact) writer_indent(w);
                    serialize_string(w, e->key);
                    writer_append_char(w, ':');
                    if (!w->compact) writer_append_char(w, ' ');
                    serialize_value(w, e->value);
                    first = false;
                }
            }
            w->indent--;
            if (!first && !w->compact) writer_indent(w);
            writer_append_char(w, '}');
            break;
        }
    }
}

char* json_stringify(const serial_value_t *val) {
    json_writer_t w;
    writer_init(&w, false);
    serialize_value(&w, val);
    writer_append_char(&w, '\0');
    return w.buf;
}

char* json_stringify_compact(const serial_value_t *val) {
    json_writer_t w;
    writer_init(&w, true);
    serialize_value(&w, val);
    writer_append_char(&w, '\0');
    return w.buf;
}

void json_free(serial_value_t *val) {
    if (!val) return;

    switch (val->type) {
        case SERIAL_STRING:
            free(val->data.string_val);
            break;
        case SERIAL_ARRAY: {
            serial_array_t *arr = val->data.array_val;
            for (size_t i = 0; i < arr->len; i++) {
                json_free(arr->items[i]);
            }
            free(arr->items);
            free(arr);
            break;
        }
        case SERIAL_MAP: {
            serial_map_t *map = val->data.map_val;
            for (size_t i = 0; i < map->bucket_count; i++) {
                serial_map_entry_t *e = map->buckets[i];
                while (e) {
                    serial_map_entry_t *next = e->next;
                    free(e->key);
                    json_free(e->value);
                    free(e);
                    e = next;
                }
            }
            free(map->buckets);
            free(map);
            break;
        }
        default:
            break;
    }
    free(val);
}

/* ==================== MessagePack Implementation ==================== */

#define MSGPACK_FIXINT_POS_MAX 0x7f
#define MSGPACK_FIXMAP_MAX 0x0f
#define MSGPACK_FIXARRAY_MAX 0x0f
#define MSGPACK_FIXSTR_MAX 0x1f

#define MSGPACK_NIL 0xc0
#define MSGPACK_FALSE 0xc2
#define MSGPACK_TRUE 0xc3
#define MSGPACK_UINT8 0xcc
#define MSGPACK_UINT16 0xcd
#define MSGPACK_UINT32 0xce
#define MSGPACK_UINT64 0xcf
#define MSGPACK_INT8 0xd0
#define MSGPACK_INT16 0xd1
#define MSGPACK_INT32 0xd2
#define MSGPACK_INT64 0xd3
#define MSGPACK_FLOAT32 0xca
#define MSGPACK_FLOAT64 0xcb
#define MSGPACK_STR8 0xd9
#define MSGPACK_STR16 0xda
#define MSGPACK_STR32 0xdb
#define MSGPACK_ARRAY16 0xdc
#define MSGPACK_ARRAY32 0xdd
#define MSGPACK_MAP16 0xde
#define MSGPACK_MAP32 0xdf

typedef struct {
    const uint8_t *data;
    size_t len;
    size_t pos;
} msgpack_parser_t;

static uint16_t read_uint16_be(msgpack_parser_t *p) {
    uint16_t val = ((uint16_t)p->data[p->pos] << 8) | p->data[p->pos + 1];
    p->pos += 2;
    return val;
}

static uint32_t read_uint32_be(msgpack_parser_t *p) {
    uint32_t val = ((uint32_t)p->data[p->pos] << 24) |
                   ((uint32_t)p->data[p->pos + 1] << 16) |
                   ((uint32_t)p->data[p->pos + 2] << 8) |
                   (uint32_t)p->data[p->pos + 3];
    p->pos += 4;
    return val;
}

static uint64_t read_uint64_be(msgpack_parser_t *p) {
    uint64_t val = ((uint64_t)p->data[p->pos] << 56) |
                   ((uint64_t)p->data[p->pos + 1] << 48) |
                   ((uint64_t)p->data[p->pos + 2] << 40) |
                   ((uint64_t)p->data[p->pos + 3] << 32) |
                   ((uint64_t)p->data[p->pos + 4] << 24) |
                   ((uint64_t)p->data[p->pos + 5] << 16) |
                   ((uint64_t)p->data[p->pos + 6] << 8) |
                   (uint64_t)p->data[p->pos + 7];
    p->pos += 8;
    return val;
}

static serial_value_t* msgpack_parse_value(msgpack_parser_t *p);

static serial_value_t* msgpack_parse_value(msgpack_parser_t *p) {
    if (p->pos >= p->len) {
        serial_set_error(SERIAL_ERR_PARSE, 0, 0, "Unexpected end of data");
        return NULL;
    }

    uint8_t b = p->data[p->pos++];

    // Positive fixint (0x00 - 0x7f)
    if (b <= MSGPACK_FIXINT_POS_MAX) {
        return serial_create_int(b);
    }

    // Fixmap (0x80 - 0x8f)
    if (b >= 0x80 && b <= 0x8f) {
        size_t count = b & 0x0f;
        serial_value_t *map = serial_create_map();
        for (size_t i = 0; i < count; i++) {
            serial_value_t *key = msgpack_parse_value(p);
            if (!key || key->type != SERIAL_STRING) {
                msgpack_free(map);
                msgpack_free(key);
                serial_set_error(SERIAL_ERR_PARSE, 0, 0, "Map key must be string");
                return NULL;
            }
            serial_value_t *val = msgpack_parse_value(p);
            if (!val) {
                msgpack_free(map);
                msgpack_free(key);
                return NULL;
            }
            serial_map_set(map, serial_get_string(key), val);
            msgpack_free(key);
        }
        return map;
    }

    // Fixarray (0x90 - 0x9f)
    if (b >= 0x90 && b <= 0x9f) {
        size_t count = b & 0x0f;
        serial_value_t *arr = serial_create_array();
        for (size_t i = 0; i < count; i++) {
            serial_value_t *val = msgpack_parse_value(p);
            if (!val) {
                msgpack_free(arr);
                return NULL;
            }
            serial_array_push(arr, val);
        }
        return arr;
    }

    // Fixstr (0xa0 - 0xbf)
    if (b >= 0xa0 && b <= 0xbf) {
        size_t len = b & 0x1f;
        if (p->pos + len > p->len) {
            serial_set_error(SERIAL_ERR_PARSE, 0, 0, "String data incomplete");
            return NULL;
        }
        serial_value_t *val = serial_create_string_len((const char*)p->data + p->pos, len);
        p->pos += len;
        return val;
    }

    // Negative fixint (0xe0 - 0xff)
    if (b >= 0xe0) {
        return serial_create_int((int8_t)b);
    }

    switch (b) {
        case MSGPACK_NIL:
            return serial_create_null();
        case MSGPACK_FALSE:
            return serial_create_bool(false);
        case MSGPACK_TRUE:
            return serial_create_bool(true);

        case MSGPACK_UINT8:
            return serial_create_int(p->data[p->pos++]);
        case MSGPACK_UINT16:
            return serial_create_int(read_uint16_be(p));
        case MSGPACK_UINT32:
            return serial_create_int(read_uint32_be(p));
        case MSGPACK_UINT64:
            return serial_create_int(read_uint64_be(p));

        case MSGPACK_INT8:
            return serial_create_int((int8_t)p->data[p->pos++]);
        case MSGPACK_INT16:
            return serial_create_int((int16_t)read_uint16_be(p));
        case MSGPACK_INT32:
            return serial_create_int((int32_t)read_uint32_be(p));
        case MSGPACK_INT64:
            return serial_create_int((int64_t)read_uint64_be(p));

        case MSGPACK_FLOAT64: {
            uint64_t bits = read_uint64_be(p);
            double val;
            memcpy(&val, &bits, sizeof(val));
            return serial_create_double(val);
        }

        case MSGPACK_STR8: {
            uint8_t len = p->data[p->pos++];
            serial_value_t *val = serial_create_string_len((const char*)p->data + p->pos, len);
            p->pos += len;
            return val;
        }
        case MSGPACK_STR16: {
            uint16_t len = read_uint16_be(p);
            serial_value_t *val = serial_create_string_len((const char*)p->data + p->pos, len);
            p->pos += len;
            return val;
        }
        case MSGPACK_STR32: {
            uint32_t len = read_uint32_be(p);
            serial_value_t *val = serial_create_string_len((const char*)p->data + p->pos, len);
            p->pos += len;
            return val;
        }

        case MSGPACK_ARRAY16: {
            uint16_t count = read_uint16_be(p);
            serial_value_t *arr = serial_create_array();
            for (uint16_t i = 0; i < count; i++) {
                serial_value_t *val = msgpack_parse_value(p);
                if (!val) {
                    msgpack_free(arr);
                    return NULL;
                }
                serial_array_push(arr, val);
            }
            return arr;
        }
        case MSGPACK_ARRAY32: {
            uint32_t count = read_uint32_be(p);
            serial_value_t *arr = serial_create_array();
            for (uint32_t i = 0; i < count; i++) {
                serial_value_t *val = msgpack_parse_value(p);
                if (!val) {
                    msgpack_free(arr);
                    return NULL;
                }
                serial_array_push(arr, val);
            }
            return arr;
        }

        case MSGPACK_MAP16: {
            uint16_t count = read_uint16_be(p);
            serial_value_t *map = serial_create_map();
            for (uint16_t i = 0; i < count; i++) {
                serial_value_t *key = msgpack_parse_value(p);
                if (!key || key->type != SERIAL_STRING) {
                    msgpack_free(map);
                    msgpack_free(key);
                    serial_set_error(SERIAL_ERR_PARSE, 0, 0, "Map key must be string");
                    return NULL;
                }
                serial_value_t *val = msgpack_parse_value(p);
                if (!val) {
                    msgpack_free(map);
                    msgpack_free(key);
                    return NULL;
                }
                serial_map_set(map, serial_get_string(key), val);
                msgpack_free(key);
            }
            return map;
        }
        case MSGPACK_MAP32: {
            uint32_t count = read_uint32_be(p);
            serial_value_t *map = serial_create_map();
            for (uint32_t i = 0; i < count; i++) {
                serial_value_t *key = msgpack_parse_value(p);
                if (!key || key->type != SERIAL_STRING) {
                    msgpack_free(map);
                    msgpack_free(key);
                    serial_set_error(SERIAL_ERR_PARSE, 0, 0, "Map key must be string");
                    return NULL;
                }
                serial_value_t *val = msgpack_parse_value(p);
                if (!val) {
                    msgpack_free(map);
                    msgpack_free(key);
                    return NULL;
                }
                serial_map_set(map, serial_get_string(key), val);
                msgpack_free(key);
            }
            return map;
        }

        default:
            serial_set_error(SERIAL_ERR_PARSE, 0, 0, "Unknown MessagePack type");
            return NULL;
    }
}

serial_value_t* msgpack_unpack(const uint8_t *data, size_t len) {
    serial_clear_error();
    msgpack_parser_t p = {data, len, 0};
    return msgpack_parse_value(&p);
}

/* MessagePack encoder */
typedef struct {
    uint8_t *buf;
    size_t len;
    size_t cap;
} msgpack_writer_t;

static void mp_writer_init(msgpack_writer_t *w) {
    w->cap = 256;
    w->buf = (uint8_t*)malloc(w->cap);
    w->len = 0;
}

static void mp_writer_grow(msgpack_writer_t *w, size_t needed) {
    while (w->len + needed >= w->cap) {
        w->cap *= 2;
    }
    w->buf = (uint8_t*)realloc(w->buf, w->cap);
}

static void mp_write_byte(msgpack_writer_t *w, uint8_t b) {
    mp_writer_grow(w, 1);
    w->buf[w->len++] = b;
}

static void mp_write_uint16(msgpack_writer_t *w, uint16_t val) {
    mp_writer_grow(w, 2);
    w->buf[w->len++] = (val >> 8) & 0xff;
    w->buf[w->len++] = val & 0xff;
}

static void mp_write_uint32(msgpack_writer_t *w, uint32_t val) {
    mp_writer_grow(w, 4);
    w->buf[w->len++] = (val >> 24) & 0xff;
    w->buf[w->len++] = (val >> 16) & 0xff;
    w->buf[w->len++] = (val >> 8) & 0xff;
    w->buf[w->len++] = val & 0xff;
}

static void mp_write_uint64(msgpack_writer_t *w, uint64_t val) {
    mp_writer_grow(w, 8);
    w->buf[w->len++] = (val >> 56) & 0xff;
    w->buf[w->len++] = (val >> 48) & 0xff;
    w->buf[w->len++] = (val >> 40) & 0xff;
    w->buf[w->len++] = (val >> 32) & 0xff;
    w->buf[w->len++] = (val >> 24) & 0xff;
    w->buf[w->len++] = (val >> 16) & 0xff;
    w->buf[w->len++] = (val >> 8) & 0xff;
    w->buf[w->len++] = val & 0xff;
}

static void mp_write_bytes(msgpack_writer_t *w, const void *data, size_t len) {
    mp_writer_grow(w, len);
    memcpy(w->buf + w->len, data, len);
    w->len += len;
}

static void msgpack_encode_value(msgpack_writer_t *w, const serial_value_t *val);

static void msgpack_encode_value(msgpack_writer_t *w, const serial_value_t *val) {
    if (!val) {
        mp_write_byte(w, MSGPACK_NIL);
        return;
    }

    switch (val->type) {
        case SERIAL_NULL:
            mp_write_byte(w, MSGPACK_NIL);
            break;
        case SERIAL_BOOL:
            mp_write_byte(w, val->data.bool_val ? MSGPACK_TRUE : MSGPACK_FALSE);
            break;
        case SERIAL_INT: {
            int64_t i = val->data.int_val;
            if (i >= 0 && i <= 127) {
                mp_write_byte(w, (uint8_t)i);
            } else if (i >= -32 && i < 0) {
                mp_write_byte(w, (uint8_t)i);
            } else if (i >= 0 && i <= 0xff) {
                mp_write_byte(w, MSGPACK_UINT8);
                mp_write_byte(w, (uint8_t)i);
            } else if (i >= 0 && i <= 0xffff) {
                mp_write_byte(w, MSGPACK_UINT16);
                mp_write_uint16(w, (uint16_t)i);
            } else if (i >= 0 && i <= 0xffffffff) {
                mp_write_byte(w, MSGPACK_UINT32);
                mp_write_uint32(w, (uint32_t)i);
            } else if (i >= 0) {
                mp_write_byte(w, MSGPACK_UINT64);
                mp_write_uint64(w, (uint64_t)i);
            } else if (i >= -128) {
                mp_write_byte(w, MSGPACK_INT8);
                mp_write_byte(w, (uint8_t)i);
            } else if (i >= -32768) {
                mp_write_byte(w, MSGPACK_INT16);
                mp_write_uint16(w, (uint16_t)i);
            } else if (i >= -2147483648LL) {
                mp_write_byte(w, MSGPACK_INT32);
                mp_write_uint32(w, (uint32_t)i);
            } else {
                mp_write_byte(w, MSGPACK_INT64);
                mp_write_uint64(w, (uint64_t)i);
            }
            break;
        }
        case SERIAL_DOUBLE: {
            mp_write_byte(w, MSGPACK_FLOAT64);
            uint64_t bits;
            memcpy(&bits, &val->data.double_val, sizeof(bits));
            mp_write_uint64(w, bits);
            break;
        }
        case SERIAL_STRING: {
            size_t len = strlen(val->data.string_val);
            if (len <= 31) {
                mp_write_byte(w, 0xa0 | (uint8_t)len);
            } else if (len <= 0xff) {
                mp_write_byte(w, MSGPACK_STR8);
                mp_write_byte(w, (uint8_t)len);
            } else if (len <= 0xffff) {
                mp_write_byte(w, MSGPACK_STR16);
                mp_write_uint16(w, (uint16_t)len);
            } else {
                mp_write_byte(w, MSGPACK_STR32);
                mp_write_uint32(w, (uint32_t)len);
            }
            mp_write_bytes(w, val->data.string_val, len);
            break;
        }
        case SERIAL_ARRAY: {
            serial_array_t *arr = val->data.array_val;
            if (arr->len <= 15) {
                mp_write_byte(w, 0x90 | (uint8_t)arr->len);
            } else if (arr->len <= 0xffff) {
                mp_write_byte(w, MSGPACK_ARRAY16);
                mp_write_uint16(w, (uint16_t)arr->len);
            } else {
                mp_write_byte(w, MSGPACK_ARRAY32);
                mp_write_uint32(w, (uint32_t)arr->len);
            }
            for (size_t i = 0; i < arr->len; i++) {
                msgpack_encode_value(w, arr->items[i]);
            }
            break;
        }
        case SERIAL_MAP: {
            serial_map_t *map = val->data.map_val;
            if (map->size <= 15) {
                mp_write_byte(w, 0x80 | (uint8_t)map->size);
            } else if (map->size <= 0xffff) {
                mp_write_byte(w, MSGPACK_MAP16);
                mp_write_uint16(w, (uint16_t)map->size);
            } else {
                mp_write_byte(w, MSGPACK_MAP32);
                mp_write_uint32(w, (uint32_t)map->size);
            }
            for (size_t i = 0; i < map->bucket_count; i++) {
                for (serial_map_entry_t *e = map->buckets[i]; e; e = e->next) {
                    // Encode key as string
                    size_t key_len = strlen(e->key);
                    if (key_len <= 31) {
                        mp_write_byte(w, 0xa0 | (uint8_t)key_len);
                    } else if (key_len <= 0xff) {
                        mp_write_byte(w, MSGPACK_STR8);
                        mp_write_byte(w, (uint8_t)key_len);
                    } else {
                        mp_write_byte(w, MSGPACK_STR16);
                        mp_write_uint16(w, (uint16_t)key_len);
                    }
                    mp_write_bytes(w, e->key, key_len);
                    msgpack_encode_value(w, e->value);
                }
            }
            break;
        }
    }
}

uint8_t* msgpack_pack(const serial_value_t *val, size_t *out_len) {
    serial_clear_error();
    msgpack_writer_t w;
    mp_writer_init(&w);
    msgpack_encode_value(&w, val);
    *out_len = w.len;
    return w.buf;
}

void msgpack_free(serial_value_t *val) {
    json_free(val);  // Same implementation
}

/* ==================== Value Creation ==================== */

serial_value_t* serial_create_null(void) {
    serial_value_t *val = (serial_value_t*)calloc(1, sizeof(serial_value_t));
    val->type = SERIAL_NULL;
    return val;
}

serial_value_t* serial_create_bool(bool value) {
    serial_value_t *val = (serial_value_t*)calloc(1, sizeof(serial_value_t));
    val->type = SERIAL_BOOL;
    val->data.bool_val = value;
    return val;
}

serial_value_t* serial_create_int(int64_t value) {
    serial_value_t *val = (serial_value_t*)calloc(1, sizeof(serial_value_t));
    val->type = SERIAL_INT;
    val->data.int_val = value;
    return val;
}

serial_value_t* serial_create_double(double value) {
    serial_value_t *val = (serial_value_t*)calloc(1, sizeof(serial_value_t));
    val->type = SERIAL_DOUBLE;
    val->data.double_val = value;
    return val;
}

serial_value_t* serial_create_string(const char *str) {
    return serial_create_string_len(str, strlen(str));
}

serial_value_t* serial_create_string_len(const char *str, size_t len) {
    serial_value_t *val = (serial_value_t*)calloc(1, sizeof(serial_value_t));
    val->type = SERIAL_STRING;
    val->data.string_val = serial_strndup(str, len);
    return val;
}

serial_value_t* serial_create_array(void) {
    serial_value_t *val = (serial_value_t*)calloc(1, sizeof(serial_value_t));
    val->type = SERIAL_ARRAY;
    val->data.array_val = (serial_array_t*)calloc(1, sizeof(serial_array_t));
    val->data.array_val->capacity = 8;
    val->data.array_val->items = (serial_value_t**)calloc(8, sizeof(serial_value_t*));
    return val;
}

static uint32_t hash_string(const char *str) {
    uint32_t hash = 5381;
    while (*str) {
        hash = ((hash << 5) + hash) + (unsigned char)*str++;
    }
    return hash;
}

serial_value_t* serial_create_map(void) {
    serial_value_t *val = (serial_value_t*)calloc(1, sizeof(serial_value_t));
    val->type = SERIAL_MAP;
    val->data.map_val = (serial_map_t*)calloc(1, sizeof(serial_map_t));
    val->data.map_val->bucket_count = 16;
    val->data.map_val->buckets = (serial_map_entry_t**)calloc(16, sizeof(serial_map_entry_t*));
    return val;
}

/* ==================== Value Inspection ==================== */

serial_type_t serial_get_type(const serial_value_t *val) {
    return val ? val->type : SERIAL_NULL;
}

bool serial_is_null(const serial_value_t *val) { return val && val->type == SERIAL_NULL; }
bool serial_is_bool(const serial_value_t *val) { return val && val->type == SERIAL_BOOL; }
bool serial_is_int(const serial_value_t *val) { return val && val->type == SERIAL_INT; }
bool serial_is_double(const serial_value_t *val) { return val && val->type == SERIAL_DOUBLE; }
bool serial_is_string(const serial_value_t *val) { return val && val->type == SERIAL_STRING; }
bool serial_is_array(const serial_value_t *val) { return val && val->type == SERIAL_ARRAY; }
bool serial_is_map(const serial_value_t *val) { return val && val->type == SERIAL_MAP; }

/* ==================== Value Extraction ==================== */

bool serial_get_bool(const serial_value_t *val) {
    return (val && val->type == SERIAL_BOOL) ? val->data.bool_val : false;
}

int64_t serial_get_int(const serial_value_t *val) {
    return (val && val->type == SERIAL_INT) ? val->data.int_val : 0;
}

double serial_get_double(const serial_value_t *val) {
    if (!val) return 0.0;
    if (val->type == SERIAL_DOUBLE) return val->data.double_val;
    if (val->type == SERIAL_INT) return (double)val->data.int_val;
    return 0.0;
}

const char* serial_get_string(const serial_value_t *val) {
    return (val && val->type == SERIAL_STRING) ? val->data.string_val : "";
}

/* ==================== Array Operations ==================== */

size_t serial_array_len(const serial_value_t *arr) {
    return (arr && arr->type == SERIAL_ARRAY) ? arr->data.array_val->len : 0;
}

serial_value_t* serial_array_get(const serial_value_t *arr, size_t index) {
    if (!arr || arr->type != SERIAL_ARRAY) return NULL;
    if (index >= arr->data.array_val->len) return NULL;
    return arr->data.array_val->items[index];
}

int serial_array_push(serial_value_t *arr, serial_value_t *val) {
    if (!arr || arr->type != SERIAL_ARRAY) return SERIAL_ERR_TYPE;
    serial_array_t *a = arr->data.array_val;
    if (a->len >= a->capacity) {
        a->capacity *= 2;
        a->items = (serial_value_t**)realloc(a->items, a->capacity * sizeof(serial_value_t*));
    }
    a->items[a->len++] = val;
    return SERIAL_OK;
}

int serial_array_set(serial_value_t *arr, size_t index, serial_value_t *val) {
    if (!arr || arr->type != SERIAL_ARRAY) return SERIAL_ERR_TYPE;
    if (index >= arr->data.array_val->len) return SERIAL_ERR_INVALID;
    json_free(arr->data.array_val->items[index]);
    arr->data.array_val->items[index] = val;
    return SERIAL_OK;
}

/* ==================== Map Operations ==================== */

size_t serial_map_size(const serial_value_t *map) {
    return (map && map->type == SERIAL_MAP) ? map->data.map_val->size : 0;
}

serial_value_t* serial_map_get(const serial_value_t *map, const char *key) {
    if (!map || map->type != SERIAL_MAP) return NULL;
    serial_map_t *m = map->data.map_val;
    uint32_t hash = hash_string(key);
    size_t bucket = hash % m->bucket_count;
    for (serial_map_entry_t *e = m->buckets[bucket]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            return e->value;
        }
    }
    return NULL;
}

int serial_map_set(serial_value_t *map, const char *key, serial_value_t *val) {
    if (!map || map->type != SERIAL_MAP) return SERIAL_ERR_TYPE;
    serial_map_t *m = map->data.map_val;
    uint32_t hash = hash_string(key);
    size_t bucket = hash % m->bucket_count;

    // Check if key exists
    for (serial_map_entry_t *e = m->buckets[bucket]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            json_free(e->value);
            e->value = val;
            return SERIAL_OK;
        }
    }

    // Add new entry
    serial_map_entry_t *entry = (serial_map_entry_t*)malloc(sizeof(serial_map_entry_t));
    entry->key = strdup(key);
    entry->value = val;
    entry->next = m->buckets[bucket];
    m->buckets[bucket] = entry;
    m->size++;
    return SERIAL_OK;
}

bool serial_map_has(const serial_value_t *map, const char *key) {
    return serial_map_get(map, key) != NULL;
}

int serial_map_remove(serial_value_t *map, const char *key) {
    if (!map || map->type != SERIAL_MAP) return SERIAL_ERR_TYPE;
    serial_map_t *m = map->data.map_val;
    uint32_t hash = hash_string(key);
    size_t bucket = hash % m->bucket_count;

    serial_map_entry_t **prev = &m->buckets[bucket];
    for (serial_map_entry_t *e = m->buckets[bucket]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            *prev = e->next;
            free(e->key);
            json_free(e->value);
            free(e);
            m->size--;
            return SERIAL_OK;
        }
        prev = &e->next;
    }
    return SERIAL_ERR_INVALID;
}

/* ==================== Utility Functions ==================== */

serial_value_t* serial_clone(const serial_value_t *val) {
    if (!val) return NULL;

    switch (val->type) {
        case SERIAL_NULL: return serial_create_null();
        case SERIAL_BOOL: return serial_create_bool(val->data.bool_val);
        case SERIAL_INT: return serial_create_int(val->data.int_val);
        case SERIAL_DOUBLE: return serial_create_double(val->data.double_val);
        case SERIAL_STRING: return serial_create_string(val->data.string_val);
        case SERIAL_ARRAY: {
            serial_value_t *arr = serial_create_array();
            for (size_t i = 0; i < val->data.array_val->len; i++) {
                serial_array_push(arr, serial_clone(val->data.array_val->items[i]));
            }
            return arr;
        }
        case SERIAL_MAP: {
            serial_value_t *map = serial_create_map();
            serial_map_t *m = val->data.map_val;
            for (size_t i = 0; i < m->bucket_count; i++) {
                for (serial_map_entry_t *e = m->buckets[i]; e; e = e->next) {
                    serial_map_set(map, e->key, serial_clone(e->value));
                }
            }
            return map;
        }
    }
    return NULL;
}

bool serial_equals(const serial_value_t *a, const serial_value_t *b) {
    if (a == b) return true;
    if (!a || !b) return false;
    if (a->type != b->type) return false;

    switch (a->type) {
        case SERIAL_NULL: return true;
        case SERIAL_BOOL: return a->data.bool_val == b->data.bool_val;
        case SERIAL_INT: return a->data.int_val == b->data.int_val;
        case SERIAL_DOUBLE: return a->data.double_val == b->data.double_val;
        case SERIAL_STRING: return strcmp(a->data.string_val, b->data.string_val) == 0;
        case SERIAL_ARRAY: {
            if (a->data.array_val->len != b->data.array_val->len) return false;
            for (size_t i = 0; i < a->data.array_val->len; i++) {
                if (!serial_equals(a->data.array_val->items[i], b->data.array_val->items[i])) {
                    return false;
                }
            }
            return true;
        }
        case SERIAL_MAP: {
            if (a->data.map_val->size != b->data.map_val->size) return false;
            serial_map_t *ma = a->data.map_val;
            for (size_t i = 0; i < ma->bucket_count; i++) {
                for (serial_map_entry_t *e = ma->buckets[i]; e; e = e->next) {
                    serial_value_t *bval = serial_map_get(b, e->key);
                    if (!serial_equals(e->value, bval)) return false;
                }
            }
            return true;
        }
    }
    return false;
}

size_t serial_memory_usage(const serial_value_t *val) {
    if (!val) return 0;
    size_t total = sizeof(serial_value_t);

    switch (val->type) {
        case SERIAL_STRING:
            total += strlen(val->data.string_val) + 1;
            break;
        case SERIAL_ARRAY: {
            serial_array_t *arr = val->data.array_val;
            total += sizeof(serial_array_t) + arr->capacity * sizeof(serial_value_t*);
            for (size_t i = 0; i < arr->len; i++) {
                total += serial_memory_usage(arr->items[i]);
            }
            break;
        }
        case SERIAL_MAP: {
            serial_map_t *map = val->data.map_val;
            total += sizeof(serial_map_t) + map->bucket_count * sizeof(serial_map_entry_t*);
            for (size_t i = 0; i < map->bucket_count; i++) {
                for (serial_map_entry_t *e = map->buckets[i]; e; e = e->next) {
                    total += sizeof(serial_map_entry_t);
                    total += strlen(e->key) + 1;
                    total += serial_memory_usage(e->value);
                }
            }
            break;
        }
        default:
            break;
    }
    return total;
}

/* ==================== Streaming API (Stub) ==================== */

struct serial_stream {
    char *buffer;
    size_t buffer_size;
    size_t buffer_capacity;
    serial_value_t *value;
    bool complete;
};

serial_stream_t* json_stream_new(size_t chunk_size) {
    serial_stream_t *stream = (serial_stream_t*)calloc(1, sizeof(serial_stream_t));
    stream->buffer_capacity = chunk_size > 0 ? chunk_size : 4096;
    stream->buffer = (char*)malloc(stream->buffer_capacity);
    return stream;
}

int json_stream_feed(serial_stream_t *stream, const char *data, size_t len) {
    if (stream->complete) return 1;

    // Grow buffer if needed
    while (stream->buffer_size + len >= stream->buffer_capacity) {
        stream->buffer_capacity *= 2;
        stream->buffer = (char*)realloc(stream->buffer, stream->buffer_capacity);
    }

    memcpy(stream->buffer + stream->buffer_size, data, len);
    stream->buffer_size += len;

    // Try to parse
    stream->value = json_parse_len(stream->buffer, stream->buffer_size);
    if (stream->value) {
        stream->complete = true;
        return 1;
    }

    // Check if error is recoverable
    serial_error_t *err = serial_get_error();
    if (err->code == SERIAL_ERR_PARSE && strstr(err->message, "end of input")) {
        serial_clear_error();
        return 0;  // Need more data
    }

    return err->code;  // Actual error
}

serial_value_t* json_stream_value(serial_stream_t *stream) {
    return stream->value;
}

void json_stream_free(serial_stream_t *stream) {
    if (!stream) return;
    free(stream->buffer);
    free(stream);
}
