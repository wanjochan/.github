/**
 * CDP JSON Parser - State Machine Implementation
 * High-performance JSON parsing with minimal allocations
 */

#include "cdp_internal.h"
#include <ctype.h>

/* JSON Parser States */
typedef enum {
    JSON_STATE_START,
    JSON_STATE_OBJECT_START,
    JSON_STATE_OBJECT_KEY,
    JSON_STATE_OBJECT_COLON,
    JSON_STATE_OBJECT_VALUE,
    JSON_STATE_OBJECT_COMMA,
    JSON_STATE_ARRAY_START,
    JSON_STATE_ARRAY_VALUE,
    JSON_STATE_ARRAY_COMMA,
    JSON_STATE_STRING,
    JSON_STATE_STRING_ESCAPE,
    JSON_STATE_NUMBER,
    JSON_STATE_TRUE,
    JSON_STATE_FALSE,
    JSON_STATE_NULL,
    JSON_STATE_END,
    JSON_STATE_ERROR
} JsonState;

/* JSON Token Types */
typedef enum {
    JSON_TOKEN_NONE,
    JSON_TOKEN_OBJECT_START,
    JSON_TOKEN_OBJECT_END,
    JSON_TOKEN_ARRAY_START,
    JSON_TOKEN_ARRAY_END,
    JSON_TOKEN_STRING,
    JSON_TOKEN_NUMBER,
    JSON_TOKEN_TRUE,
    JSON_TOKEN_FALSE,
    JSON_TOKEN_NULL,
    JSON_TOKEN_COLON,
    JSON_TOKEN_COMMA
} JsonTokenType;

/* JSON Parser Context */
typedef struct {
    const char *input;
    size_t pos;
    size_t len;
    JsonState state;
    JsonState state_stack[32];
    int stack_depth;
    char *output;
    size_t output_size;
    size_t output_pos;
    int depth;
    char current_key[256];
    int in_target_object;
} JsonParser;

/* Token structure */
typedef struct {
    JsonTokenType type;
    const char *start;
    size_t length;
    union {
        double number;
        int boolean;
    } value;
} JsonToken;

/* Forward declarations */
static int json_parse_value(JsonParser *parser);
static int json_parse_object(JsonParser *parser);
static int json_parse_array(JsonParser *parser);
static int json_parse_string(JsonParser *parser, char *output, size_t size);
static int json_parse_number(JsonParser *parser, double *value);
static int json_parse_literal(JsonParser *parser, const char *literal, int value);
static void json_skip_whitespace(JsonParser *parser);

/* Initialize parser */
static void json_parser_init(JsonParser *parser, const char *input, size_t len) {
    parser->input = input;
    parser->pos = 0;
    parser->len = len > 0 ? len : strlen(input);
    parser->state = JSON_STATE_START;
    parser->stack_depth = 0;
    parser->depth = 0;
    parser->output_pos = 0;
    parser->in_target_object = 0;
    parser->current_key[0] = '\0';
}

/* Get current character */
static inline char json_current(JsonParser *parser) {
    return parser->pos < parser->len ? parser->input[parser->pos] : '\0';
}

/* Advance to next character */
static inline void json_advance(JsonParser *parser) {
    if (parser->pos < parser->len) {
        parser->pos++;
    }
}

/* Skip whitespace */
static void json_skip_whitespace(JsonParser *parser) {
    while (parser->pos < parser->len) {
        char c = json_current(parser);
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            break;
        }
        json_advance(parser);
    }
}

/* Parse a JSON value */
static int json_parse_value(JsonParser *parser) {
    json_skip_whitespace(parser);
    
    char c = json_current(parser);
    
    switch (c) {
        case '{':
            return json_parse_object(parser);
        case '[':
            return json_parse_array(parser);
        case '"':
            return json_parse_string(parser, NULL, 0);
        case 't':
            return json_parse_literal(parser, "true", 1);
        case 'f':
            return json_parse_literal(parser, "false", 0);
        case 'n':
            return json_parse_literal(parser, "null", 0);
        case '-':
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return json_parse_number(parser, NULL);
        default:
            parser->state = JSON_STATE_ERROR;
            return -1;
    }
}

/* Parse JSON object */
static int json_parse_object(JsonParser *parser) {
    if (json_current(parser) != '{') {
        return -1;
    }
    json_advance(parser);
    parser->depth++;
    
    json_skip_whitespace(parser);
    
    // Empty object
    if (json_current(parser) == '}') {
        json_advance(parser);
        parser->depth--;
        return 0;
    }
    
    while (1) {
        json_skip_whitespace(parser);
        
        // Parse key
        char key[256];
        if (json_parse_string(parser, key, sizeof(key)) < 0) {
            return -1;
        }
        
        json_skip_whitespace(parser);
        
        // Expect colon
        if (json_current(parser) != ':') {
            return -1;
        }
        json_advance(parser);
        
        json_skip_whitespace(parser);
        
        // Parse value
        if (json_parse_value(parser) < 0) {
            return -1;
        }
        
        json_skip_whitespace(parser);
        
        char c = json_current(parser);
        if (c == '}') {
            json_advance(parser);
            parser->depth--;
            return 0;
        } else if (c == ',') {
            json_advance(parser);
        } else {
            return -1;
        }
    }
}

/* Parse JSON array */
static int json_parse_array(JsonParser *parser) {
    if (json_current(parser) != '[') {
        return -1;
    }
    json_advance(parser);
    parser->depth++;
    
    json_skip_whitespace(parser);
    
    // Empty array
    if (json_current(parser) == ']') {
        json_advance(parser);
        parser->depth--;
        return 0;
    }
    
    while (1) {
        json_skip_whitespace(parser);
        
        // Parse value
        if (json_parse_value(parser) < 0) {
            return -1;
        }
        
        json_skip_whitespace(parser);
        
        char c = json_current(parser);
        if (c == ']') {
            json_advance(parser);
            parser->depth--;
            return 0;
        } else if (c == ',') {
            json_advance(parser);
        } else {
            return -1;
        }
    }
}

/* Parse JSON string */
static int json_parse_string(JsonParser *parser, char *output, size_t size) {
    if (json_current(parser) != '"') {
        return -1;
    }
    json_advance(parser);
    
    size_t start = parser->pos;
    size_t out_pos = 0;
    
    while (parser->pos < parser->len) {
        char c = json_current(parser);
        
        if (c == '"') {
            json_advance(parser);
            if (output && out_pos < size) {
                output[out_pos] = '\0';
            }
            return out_pos;
        } else if (c == '\\') {
            json_advance(parser);
            if (parser->pos >= parser->len) {
                return -1;
            }
            
            c = json_current(parser);
            char decoded;
            
            switch (c) {
                case '"': decoded = '"'; break;
                case '\\': decoded = '\\'; break;
                case '/': decoded = '/'; break;
                case 'b': decoded = '\b'; break;
                case 'f': decoded = '\f'; break;
                case 'n': decoded = '\n'; break;
                case 'r': decoded = '\r'; break;
                case 't': decoded = '\t'; break;
                case 'u':
                    // Unicode escape - simplified handling
                    json_advance(parser);
                    for (int i = 0; i < 4 && parser->pos < parser->len; i++) {
                        json_advance(parser);
                    }
                    decoded = '?'; // Placeholder for unicode
                    break;
                default:
                    return -1;
            }
            
            if (output && out_pos < size - 1) {
                output[out_pos++] = decoded;
            }
            json_advance(parser);
        } else {
            if (output && out_pos < size - 1) {
                output[out_pos++] = c;
            }
            json_advance(parser);
        }
    }
    
    return -1; // Unterminated string
}

/* Parse JSON number */
static int json_parse_number(JsonParser *parser, double *value) {
    size_t start = parser->pos;
    
    // Optional minus
    if (json_current(parser) == '-') {
        json_advance(parser);
    }
    
    // Integer part
    if (json_current(parser) == '0') {
        json_advance(parser);
    } else if (isdigit(json_current(parser))) {
        while (isdigit(json_current(parser))) {
            json_advance(parser);
        }
    } else {
        return -1;
    }
    
    // Fractional part
    if (json_current(parser) == '.') {
        json_advance(parser);
        if (!isdigit(json_current(parser))) {
            return -1;
        }
        while (isdigit(json_current(parser))) {
            json_advance(parser);
        }
    }
    
    // Exponent part
    if (json_current(parser) == 'e' || json_current(parser) == 'E') {
        json_advance(parser);
        if (json_current(parser) == '+' || json_current(parser) == '-') {
            json_advance(parser);
        }
        if (!isdigit(json_current(parser))) {
            return -1;
        }
        while (isdigit(json_current(parser))) {
            json_advance(parser);
        }
    }
    
    if (value) {
        char num_str[64];
        size_t len = parser->pos - start;
        if (len < sizeof(num_str)) {
            memcpy(num_str, parser->input + start, len);
            num_str[len] = '\0';
            *value = strtod(num_str, NULL);
        }
    }
    
    return 0;
}

/* Parse JSON literal (true, false, null) */
static int json_parse_literal(JsonParser *parser, const char *literal, int value) {
    size_t lit_len = strlen(literal);
    
    if (parser->pos + lit_len > parser->len) {
        return -1;
    }
    
    if (strncmp(parser->input + parser->pos, literal, lit_len) != 0) {
        return -1;
    }
    
    parser->pos += lit_len;
    return value;
}

/* Public API Functions */

/* Fast JSON value extraction */
int cdp_json_get_string_fast(const char *json, const char *key, 
                             char *value, size_t value_size) {
    if (!json || !key || !value || value_size == 0) {
        return -1;
    }
    
    JsonParser parser;
    json_parser_init(&parser, json, 0);
    
    // Build search pattern
    char pattern[512];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    
    // Quick search for key
    const char *pos = strstr(json, pattern);
    if (!pos) {
        return -1;
    }
    
    // Set parser position after the colon
    parser.pos = pos - json + strlen(pattern);
    json_skip_whitespace(&parser);
    
    // Parse the value
    if (json_current(&parser) == '"') {
        return json_parse_string(&parser, value, value_size);
    }
    
    return -1;
}

/* Fast JSON integer extraction */
int cdp_json_get_int_fast(const char *json, const char *key) {
    if (!json || !key) {
        return 0;
    }
    
    JsonParser parser;
    json_parser_init(&parser, json, 0);
    
    char pattern[512];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    
    const char *pos = strstr(json, pattern);
    if (!pos) {
        return 0;
    }
    
    parser.pos = pos - json + strlen(pattern);
    json_skip_whitespace(&parser);
    
    double value;
    if (json_parse_number(&parser, &value) == 0) {
        return (int)value;
    }
    
    return 0;
}

/* Stream JSON parsing for large responses */
typedef struct {
    char *buffer;
    size_t size;
    size_t pos;
    size_t write_pos;
    void (*callback)(const char *key, const char *value, void *user_data);
    void *user_data;
} JsonStream;

/* Initialize JSON stream parser */
JsonStream* cdp_json_stream_init(size_t buffer_size,
                                 void (*callback)(const char*, const char*, void*),
                                 void *user_data) {
    JsonStream *stream = malloc(sizeof(JsonStream));
    if (!stream) {
        return NULL;
    }
    
    stream->buffer = malloc(buffer_size);
    if (!stream->buffer) {
        free(stream);
        return NULL;
    }
    
    stream->size = buffer_size;
    stream->pos = 0;
    stream->write_pos = 0;
    stream->callback = callback;
    stream->user_data = user_data;
    
    return stream;
}

/* Feed data to stream parser */
int cdp_json_stream_feed(JsonStream *stream, const char *data, size_t len) {
    if (!stream || !data) {
        return -1;
    }
    
    // Ensure we have space
    if (stream->write_pos + len > stream->size) {
        // Compact buffer
        if (stream->pos > 0) {
            memmove(stream->buffer, stream->buffer + stream->pos, 
                   stream->write_pos - stream->pos);
            stream->write_pos -= stream->pos;
            stream->pos = 0;
        }
        
        // Still not enough space
        if (stream->write_pos + len > stream->size) {
            return -1;
        }
    }
    
    // Copy new data
    memcpy(stream->buffer + stream->write_pos, data, len);
    stream->write_pos += len;
    
    // Try to parse complete JSON objects
    JsonParser parser;
    json_parser_init(&parser, stream->buffer + stream->pos, 
                    stream->write_pos - stream->pos);
    
    while (parser.pos < parser.len) {
        size_t start = parser.pos;
        
        if (json_parse_value(&parser) == 0) {
            // Found complete value
            if (stream->callback) {
                // Extract and callback (simplified)
                stream->callback("key", "value", stream->user_data);
            }
            
            stream->pos += parser.pos;
            json_parser_init(&parser, stream->buffer + stream->pos,
                           stream->write_pos - stream->pos);
        } else {
            // Incomplete, wait for more data
            break;
        }
    }
    
    return 0;
}

/* Cleanup stream parser */
void cdp_json_stream_free(JsonStream *stream) {
    if (stream) {
        free(stream->buffer);
        free(stream);
    }
}

/* Validate JSON string */
int cdp_json_validate(const char *json) {
    if (!json) {
        return 0;
    }
    
    JsonParser parser;
    json_parser_init(&parser, json, 0);
    
    return json_parse_value(&parser) == 0 && parser.pos == parser.len;
}

/* Pretty print JSON (simplified) */
int cdp_json_pretty_print(const char *json, char *output, size_t output_size) {
    if (!json || !output || output_size == 0) {
        return -1;
    }
    
    size_t in_pos = 0;
    size_t out_pos = 0;
    int indent = 0;
    int in_string = 0;
    
    while (json[in_pos] && out_pos < output_size - 1) {
        char c = json[in_pos++];
        
        if (!in_string) {
            switch (c) {
                case '{':
                case '[':
                    output[out_pos++] = c;
                    if (out_pos < output_size - 1) output[out_pos++] = '\n';
                    indent++;
                    for (int i = 0; i < indent * 2 && out_pos < output_size - 1; i++) {
                        output[out_pos++] = ' ';
                    }
                    break;
                    
                case '}':
                case ']':
                    if (out_pos < output_size - 1) output[out_pos++] = '\n';
                    indent--;
                    for (int i = 0; i < indent * 2 && out_pos < output_size - 1; i++) {
                        output[out_pos++] = ' ';
                    }
                    output[out_pos++] = c;
                    break;
                    
                case ',':
                    output[out_pos++] = c;
                    if (out_pos < output_size - 1) output[out_pos++] = '\n';
                    for (int i = 0; i < indent * 2 && out_pos < output_size - 1; i++) {
                        output[out_pos++] = ' ';
                    }
                    break;
                    
                case ':':
                    output[out_pos++] = c;
                    if (out_pos < output_size - 1) output[out_pos++] = ' ';
                    break;
                    
                case '"':
                    in_string = 1;
                    output[out_pos++] = c;
                    break;
                    
                default:
                    if (!isspace(c)) {
                        output[out_pos++] = c;
                    }
                    break;
            }
        } else {
            output[out_pos++] = c;
            if (c == '"' && json[in_pos - 2] != '\\') {
                in_string = 0;
            }
        }
    }
    
    output[out_pos] = '\0';
    return out_pos;
}