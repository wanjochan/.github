/**
 * cdp_quickjs_json.c - QuickJS-based JSON processing for CDP
 *
 * Replaces ugly strstr() parsing with elegant JavaScript JSON handling
 */

#include "cdp_quickjs.h"
#include "quickjs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Global QuickJS context for JSON processing */
static CDPQuickJSContext *g_json_ctx = NULL;

/* Initialize JSON processing context */
int cdp_json_init(void) {
    if (g_json_ctx) return 0;  /* Already initialized */

    /* Use BALANCED config for JSON processing */
    g_json_ctx = cdp_qjs_create_context(&CDP_QJS_CONFIG_BALANCED);
    if (!g_json_ctx) {
        fprintf(stderr, "Failed to create QuickJS context for JSON\n");
        return -1;
    }

    /* Add helper functions */
    const char *helpers =
        "function extractValue(json, path) {"
        "  try {"
        "    const obj = typeof json === 'string' ? JSON.parse(json) : json;"
        "    const parts = path.split('.');"
        "    let result = obj;"
        "    for (const part of parts) {"
        "      if (result === null || result === undefined) return null;"
        "      result = result[part];"
        "    }"
        "    return result;"
        "  } catch(e) {"
        "    return null;"
        "  }"
        "}"
        ""
        "function findInJson(json, key) {"
        "  try {"
        "    const obj = typeof json === 'string' ? JSON.parse(json) : json;"
        "    function search(o, k) {"
        "      if (!o || typeof o !== 'object') return null;"
        "      if (k in o) return o[k];"
        "      for (const v of Object.values(o)) {"
        "        const result = search(v, k);"
        "        if (result !== null) return result;"
        "      }"
        "      return null;"
        "    }"
        "    return search(obj, key);"
        "  } catch(e) {"
        "    return null;"
        "  }"
        "}";

    char result[256];
    if (cdp_qjs_eval(g_json_ctx, helpers, result, sizeof(result)) < 0) {
        fprintf(stderr, "Failed to initialize JSON helpers\n");
        cdp_qjs_destroy_context(g_json_ctx);
        g_json_ctx = NULL;
        return -1;
    }

    return 0;
}

/* Cleanup JSON processing context */
void cdp_json_cleanup(void) {
    if (g_json_ctx) {
        cdp_qjs_destroy_context(g_json_ctx);
        g_json_ctx = NULL;
    }
}

/* Parse JSON and extract a field value */
int cdp_json_get_string(const char *json, const char *field, char *out, size_t out_size) {
    if (!g_json_ctx && cdp_json_init() < 0) return -1;
    if (!json || !field || !out) return -1;

    /* Build JavaScript code to extract field */
    char *code = malloc(strlen(json) + strlen(field) + 256);
    if (!code) return -1;

    /* Escape the JSON string for JavaScript */
    char *escaped_json = malloc(strlen(json) * 2 + 1);
    if (!escaped_json) {
        free(code);
        return -1;
    }

    /* Simple escape - replace \ with \\ and " with \" */
    const char *src = json;
    char *dst = escaped_json;
    while (*src) {
        if (*src == '\\') {
            *dst++ = '\\';
            *dst++ = '\\';
        } else if (*src == '"') {
            *dst++ = '\\';
            *dst++ = '"';
        } else if (*src == '\n') {
            *dst++ = '\\';
            *dst++ = 'n';
        } else if (*src == '\r') {
            *dst++ = '\\';
            *dst++ = 'r';
        } else if (*src == '\t') {
            *dst++ = '\\';
            *dst++ = 't';
        } else {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';

    sprintf(code,
        "(() => {"
        "  try {"
        "    const obj = JSON.parse(\"%s\");"
        "    const value = extractValue(obj, '%s');"
        "    return value === null || value === undefined ? '' : String(value);"
        "  } catch(e) {"
        "    return '';"
        "  }"
        "})()", escaped_json, field);

    int ret = cdp_qjs_eval(g_json_ctx, code, out, out_size);

    free(escaped_json);
    free(code);

    return ret;
}

/* Parse JSON and extract integer value */
int cdp_json_get_int(const char *json, const char *field, int *out) {
    char value[64];
    if (cdp_json_get_string(json, field, value, sizeof(value)) < 0) {
        return -1;
    }

    if (strlen(value) == 0) return -1;

    *out = atoi(value);
    return 0;
}

/* Parse JSON and extract boolean value */
int cdp_json_get_bool(const char *json, const char *field, int *out) {
    char value[32];
    if (cdp_json_get_string(json, field, value, sizeof(value)) < 0) {
        return -1;
    }

    *out = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
    return 0;
}

/* Check if JSON contains a field */
int cdp_json_has_field(const char *json, const char *field) {
    if (!g_json_ctx && cdp_json_init() < 0) return 0;
    if (!json || !field) return 0;

    char code[4096];
    snprintf(code, sizeof(code),
        "(() => {"
        "  try {"
        "    const obj = JSON.parse(`%s`);"
        "    return '%s' in obj;"
        "  } catch(e) {"
        "    return false;"
        "  }"
        "})()", json, field);

    char result[32];
    if (cdp_qjs_eval(g_json_ctx, code, result, sizeof(result)) < 0) {
        return 0;
    }

    return strcmp(result, "true") == 0;
}

/* Extract nested value using dot notation (e.g., "result.value.data") */
int cdp_json_get_nested(const char *json, const char *path, char *out, size_t out_size) {
    if (!g_json_ctx && cdp_json_init() < 0) return -1;

    return cdp_json_get_string(json, path, out, out_size);
}

/* Search for a key anywhere in JSON and return its value */
int cdp_json_find_key(const char *json, const char *key, char *out, size_t out_size) {
    if (!g_json_ctx && cdp_json_init() < 0) return -1;
    if (!json || !key || !out) return -1;

    char code[8192];
    snprintf(code, sizeof(code),
        "(() => {"
        "  const result = findInJson(`%s`, '%s');"
        "  return result === null ? '' : String(result);"
        "})()", json, key);

    return cdp_qjs_eval(g_json_ctx, code, out, out_size);
}

/* Pretty print JSON */
int cdp_json_beautify(const char *json, char *out, size_t out_size) {
    if (!g_json_ctx && cdp_json_init() < 0) return -1;
    if (!json || !out) return -1;

    char *code = malloc(strlen(json) + 256);
    if (!code) return -1;

    sprintf(code,
        "(() => {"
        "  try {"
        "    const obj = JSON.parse(`%s`);"
        "    return JSON.stringify(obj, null, 2);"
        "  } catch(e) {"
        "    return 'Invalid JSON';"
        "  }"
        "})()", json);

    int ret = cdp_qjs_eval(g_json_ctx, code, out, out_size);
    free(code);

    return ret;
}

/* Validate JSON string */
int cdp_json_is_valid(const char *json) {
    if (!g_json_ctx && cdp_json_init() < 0) return 0;
    if (!json) return 0;

    char code[256];
    snprintf(code, sizeof(code),
        "(() => {"
        "  try {"
        "    JSON.parse(`%s`);"
        "    return true;"
        "  } catch(e) {"
        "    return false;"
        "  }"
        "})()", json);

    char result[32];
    if (cdp_qjs_eval(g_json_ctx, code, result, sizeof(result)) < 0) {
        return 0;
    }

    return strcmp(result, "true") == 0;
}

/* Build JSON from key-value pairs */
int cdp_json_build(const char *pairs[][2], int count, char *out, size_t out_size) {
    if (!g_json_ctx && cdp_json_init() < 0) return -1;
    if (!pairs || !out || count <= 0) return -1;

    char code[4096] = "(() => { const obj = {";
    int offset = strlen(code);

    for (int i = 0; i < count; i++) {
        if (i > 0) {
            offset += snprintf(code + offset, sizeof(code) - offset, ",");
        }
        offset += snprintf(code + offset, sizeof(code) - offset,
                          "'%s': '%s'", pairs[i][0], pairs[i][1]);
    }

    offset += snprintf(code + offset, sizeof(code) - offset,
                      "}; return JSON.stringify(obj); })()");

    return cdp_qjs_eval(g_json_ctx, code, out, out_size);
}

/* Extract array from JSON */
int cdp_json_get_array_size(const char *json, const char *field) {
    if (!g_json_ctx && cdp_json_init() < 0) return -1;

    char code[4096];
    snprintf(code, sizeof(code),
        "(() => {"
        "  try {"
        "    const obj = JSON.parse(`%s`);"
        "    const arr = obj['%s'];"
        "    return Array.isArray(arr) ? arr.length : -1;"
        "  } catch(e) {"
        "    return -1;"
        "  }"
        "})()", json, field);

    char result[32];
    if (cdp_qjs_eval(g_json_ctx, code, result, sizeof(result)) < 0) {
        return -1;
    }

    return atoi(result);
}

/* Get array element at index */
int cdp_json_get_array_element(const char *json, const char *field, int index, char *out, size_t out_size) {
    if (!g_json_ctx && cdp_json_init() < 0) return -1;

    char path[256];
    snprintf(path, sizeof(path), "%s.%d", field, index);

    return cdp_json_get_nested(json, path, out, out_size);
}