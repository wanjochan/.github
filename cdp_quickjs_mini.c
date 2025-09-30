/**
 * cdp_quickjs_mini.c - Minimal QuickJS JSON parser without full QuickJS dependency
 *
 * This is a lightweight JSON parser that doesn't require linking QuickJS library
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Simple JSON parser for CDP - no external dependencies */

typedef struct {
    const char *str;
    int pos;
    int len;
} json_parser_t;

static void skip_whitespace(json_parser_t *p) {
    while (p->pos < p->len && isspace(p->str[p->pos])) {
        p->pos++;
    }
}

static int parse_string(json_parser_t *p, char *out, int max_len) {
    if (p->str[p->pos] != '"') return -1;
    p->pos++;

    int out_pos = 0;
    while (p->pos < p->len && p->str[p->pos] != '"') {
        if (p->str[p->pos] == '\\' && p->pos + 1 < p->len) {
            p->pos++;
            switch (p->str[p->pos]) {
                case 'n': if (out_pos < max_len - 1) out[out_pos++] = '\n'; break;
                case 't': if (out_pos < max_len - 1) out[out_pos++] = '\t'; break;
                case 'r': if (out_pos < max_len - 1) out[out_pos++] = '\r'; break;
                case '"': if (out_pos < max_len - 1) out[out_pos++] = '"'; break;
                case '\\': if (out_pos < max_len - 1) out[out_pos++] = '\\'; break;
                default: if (out_pos < max_len - 1) out[out_pos++] = p->str[p->pos]; break;
            }
        } else {
            if (out_pos < max_len - 1) out[out_pos++] = p->str[p->pos];
        }
        p->pos++;
    }

    if (p->pos >= p->len) return -1;
    p->pos++; // Skip closing quote
    out[out_pos] = '\0';
    return 0;
}

static int find_field(const char *json, const char *field, json_parser_t *result) {
    json_parser_t p = {json, 0, strlen(json)};
    char key[256];

    // Find object start
    while (p.pos < p.len && p.str[p.pos] != '{') p.pos++;
    if (p.pos >= p.len) return -1;
    p.pos++;

    while (p.pos < p.len) {
        skip_whitespace(&p);

        // Parse key
        if (parse_string(&p, key, sizeof(key)) < 0) break;

        skip_whitespace(&p);
        if (p.str[p.pos] != ':') break;
        p.pos++;
        skip_whitespace(&p);

        // Check if this is our field
        if (strcmp(key, field) == 0) {
            result->str = json;
            result->pos = p.pos;
            result->len = p.len;
            return 0;
        }

        // Skip value
        int depth = 0;
        int in_string = 0;
        while (p.pos < p.len) {
            if (!in_string) {
                if (p.str[p.pos] == '"') {
                    in_string = 1;
                } else if (p.str[p.pos] == '{' || p.str[p.pos] == '[') {
                    depth++;
                } else if (p.str[p.pos] == '}' || p.str[p.pos] == ']') {
                    if (depth == 0) break;
                    depth--;
                } else if (p.str[p.pos] == ',' && depth == 0) {
                    p.pos++;
                    break;
                }
            } else {
                if (p.str[p.pos] == '"' && p.str[p.pos-1] != '\\') {
                    in_string = 0;
                }
            }
            p.pos++;
        }
    }

    return -1;
}

/* Public API - Compatible with cdp_quickjs.h */

int cdp_json_init(void) {
    /* No initialization needed for mini parser */
    return 0;
}

void cdp_json_cleanup(void) {
    /* No cleanup needed */
}

int cdp_json_get_string(const char *json, const char *field, char *out, size_t out_size) {
    json_parser_t p;

    /* Handle nested paths */
    char *path_copy = strdup(field);
    char *token = strtok(path_copy, ".");
    const char *current_json = json;
    char temp_json[8192];

    while (token) {
        if (find_field(current_json, token, &p) < 0) {
            free(path_copy);
            return -1;
        }

        /* Check if value is a string */
        if (p.str[p.pos] == '"') {
            char *next_token = strtok(NULL, ".");
            if (next_token == NULL) {
                /* This is the final value */
                int ret = parse_string(&p, out, out_size);
                free(path_copy);
                return ret;
            }
        } else if (p.str[p.pos] == '{') {
            /* Extract nested object */
            int depth = 1;
            int start = p.pos;
            p.pos++;

            while (p.pos < p.len && depth > 0) {
                if (p.str[p.pos] == '{') depth++;
                else if (p.str[p.pos] == '}') depth--;
                p.pos++;
            }

            int len = p.pos - start;
            if (len >= sizeof(temp_json)) {
                free(path_copy);
                return -1;
            }

            memcpy(temp_json, p.str + start, len);
            temp_json[len] = '\0';
            current_json = temp_json;
        } else {
            /* Not a string or object - extract as is */
            int start = p.pos;
            while (p.pos < p.len && p.str[p.pos] != ',' && p.str[p.pos] != '}') {
                p.pos++;
            }

            int len = p.pos - start;
            if (len >= out_size) len = out_size - 1;
            memcpy(out, p.str + start, len);
            out[len] = '\0';

            /* Trim whitespace */
            while (len > 0 && isspace(out[len-1])) {
                out[--len] = '\0';
            }

            free(path_copy);
            return 0;
        }

        token = strtok(NULL, ".");
    }

    free(path_copy);
    return -1;
}

int cdp_json_get_int(const char *json, const char *field, int *out) {
    char value[64];
    if (cdp_json_get_string(json, field, value, sizeof(value)) < 0) {
        return -1;
    }
    *out = atoi(value);
    return 0;
}

int cdp_json_get_bool(const char *json, const char *field, int *out) {
    char value[32];
    if (cdp_json_get_string(json, field, value, sizeof(value)) < 0) {
        return -1;
    }
    *out = (strcmp(value, "true") == 0);
    return 0;
}

int cdp_json_get_nested(const char *json, const char *path, char *out, size_t out_size) {
    return cdp_json_get_string(json, path, out, out_size);
}

int cdp_json_beautify(const char *json, char *out, size_t out_size) {
    /* Simple beautification - just copy for now */
    strncpy(out, json, out_size - 1);
    out[out_size - 1] = '\0';
    return 0;
}