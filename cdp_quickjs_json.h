/**
 * cdp_quickjs_json.h - QuickJS JSON processing interface
 */

#ifndef CDP_QUICKJS_JSON_H
#define CDP_QUICKJS_JSON_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Initialize/cleanup JSON processing context */
int cdp_json_init(void);
void cdp_json_cleanup(void);

/* Basic JSON field extraction */
int cdp_json_get_string(const char *json, const char *field, char *out, size_t out_size);
int cdp_json_get_int(const char *json, const char *field, int *out);
int cdp_json_get_bool(const char *json, const char *field, int *out);

/* Advanced JSON operations */
int cdp_json_has_field(const char *json, const char *field);
int cdp_json_get_nested(const char *json, const char *path, char *out, size_t out_size);
int cdp_json_find_key(const char *json, const char *key, char *out, size_t out_size);

/* JSON utilities */
int cdp_json_beautify(const char *json, char *out, size_t out_size);
int cdp_json_is_valid(const char *json);
int cdp_json_build(const char *pairs[][2], int count, char *out, size_t out_size);

/* Array operations */
int cdp_json_get_array_size(const char *json, const char *field);
int cdp_json_get_array_element(const char *json, const char *field, int index, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif /* CDP_QUICKJS_JSON_H */