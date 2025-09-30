#include "cdp_json.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void jb_grow(JSONBuilder *jb, size_t need) {
    if (jb->len + need + 1 <= jb->cap) return;
    size_t ncap = jb->cap ? jb->cap : 256;
    while (jb->len + need + 1 > ncap) ncap *= 2;
    char *nbuf = (char*)realloc(jb->buf, ncap);
    if (!nbuf) return;
    jb->buf = nbuf; jb->cap = ncap;
}

static void jb_put(JSONBuilder *jb, const char *s) {
    size_t n = strlen(s);
    jb_grow(jb, n);
    memcpy(jb->buf + jb->len, s, n);
    jb->len += n; jb->buf[jb->len] = '\0';
}

static void jb_putc(JSONBuilder *jb, char c) {
    jb_grow(jb, 1);
    jb->buf[jb->len++] = c; jb->buf[jb->len] = '\0';
}

static void jb_comma_if_needed(JSONBuilder *jb) {
    if (jb->depth > 0 && jb->need_comma[jb->depth - 1]) {
        jb_putc(jb, ',');
    } else if (jb->depth > 0) {
        jb->need_comma[jb->depth - 1] = 1;
    }
}

static void jb_escape(JSONBuilder *jb, const char *s) {
    jb_putc(jb, '"');
    for (const char *p = s; *p; ++p) {
        switch (*p) {
            case '"': jb_put(jb, "\\\""); break;
            case '\\': jb_put(jb, "\\\\"); break;
            case '\n': jb_put(jb, "\\n"); break;
            case '\r': jb_put(jb, "\\r"); break;
            case '\t': jb_put(jb, "\\t"); break;
            default:
                if ((unsigned char)*p < 32) {
                    char tmp[8];
                    snprintf(tmp, sizeof(tmp), "\\u%04x", (unsigned char)*p);
                    jb_put(jb, tmp);
                } else {
                    jb_putc(jb, *p);
                }
        }
    }
    jb_putc(jb, '"');
}

JSONBuilder* jb_new(size_t initial) {
    JSONBuilder *jb = (JSONBuilder*)calloc(1, sizeof(JSONBuilder));
    if (!jb) return NULL;
    jb->cap = initial ? initial : 256;
    jb->buf = (char*)malloc(jb->cap);
    if (!jb->buf) { free(jb); return NULL; }
    jb->buf[0] = '\0'; jb->len = 0; jb->depth = 0;
    return jb;
}

void jb_free(JSONBuilder **pjb) { if (pjb && *pjb) { free((*pjb)->buf); free(*pjb); *pjb = NULL; } }

const char* jb_str(JSONBuilder *jb) { return jb ? jb->buf : NULL; }

void jb_begin_object(JSONBuilder *jb) { jb_comma_if_needed(jb); jb_putc(jb, '{'); jb->need_comma[jb->depth++] = 0; }
void jb_end_object(JSONBuilder *jb) { jb_putc(jb, '}'); if (jb->depth>0) jb->need_comma[jb->depth-1]=1; if (jb->depth>0) jb->depth--; }
void jb_begin_array(JSONBuilder *jb) { jb_comma_if_needed(jb); jb_putc(jb, '['); jb->need_comma[jb->depth++] = 0; }
void jb_end_array(JSONBuilder *jb) { jb_putc(jb, ']'); if (jb->depth>0) jb->need_comma[jb->depth-1]=1; if (jb->depth>0) jb->depth--; }

void jb_key(JSONBuilder *jb, const char *key) { jb_comma_if_needed(jb); jb_escape(jb, key); jb_putc(jb, ':'); }
void jb_add_string(JSONBuilder *jb, const char *key, const char *value) { jb_key(jb, key); jb_escape(jb, value ? value : ""); }
void jb_add_int(JSONBuilder *jb, const char *key, int value) { jb_key(jb, key); char tmp[64]; snprintf(tmp, sizeof(tmp), "%d", value); jb_put(jb, tmp); }
void jb_add_bool(JSONBuilder *jb, const char *key, int value) { jb_key(jb, key); jb_put(jb, value?"true":"false"); }
void jb_add_raw(JSONBuilder *jb, const char *key, const char *raw_json) { jb_key(jb, key); jb_put(jb, raw_json?raw_json:"null"); }

