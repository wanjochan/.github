/**
 * Simple JSON Builder (minimal, for internal use)
 */
#ifndef CDP_JSON_H
#define CDP_JSON_H

#include <stddef.h>

typedef struct {
    char *buf;
    size_t cap;
    size_t len;
    int stack[32];
    int depth;
    int need_comma[32];
} JSONBuilder;

JSONBuilder* jb_new(size_t initial);
void jb_free(JSONBuilder **pjb);
const char* jb_str(JSONBuilder *jb);

void jb_begin_object(JSONBuilder *jb);
void jb_end_object(JSONBuilder *jb);
void jb_begin_array(JSONBuilder *jb);
void jb_end_array(JSONBuilder *jb);

void jb_key(JSONBuilder *jb, const char *key);
void jb_add_string(JSONBuilder *jb, const char *key, const char *value);
void jb_add_int(JSONBuilder *jb, const char *key, int value);
void jb_add_bool(JSONBuilder *jb, const char *key, int value);
void jb_add_raw(JSONBuilder *jb, const char *key, const char *raw_json);

#endif /* CDP_JSON_H */

