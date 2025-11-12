#ifndef COSMORUN_QUERYSTRING_H
#define COSMORUN_QUERYSTRING_H

/*
 * mod_querystring - Node.js-style query string parsing for cosmorun
 *
 * Provides URL query string parsing and encoding similar to Node.js querystring module:
 * - Parse query strings into hashmaps
 * - Stringify hashmaps to query strings
 * - Percent encoding/decoding (RFC 3986)
 * - Custom separator support
 *
 * v2 System Layer: Uses shim_* API instead of direct libc calls
 * - Memory: shim_malloc, shim_free, shim_calloc, shim_realloc
 * - Strings: shim_strlen, shim_strchr, shim_strdup
 */

#include "mod_std.h"

/* ==================== Query String Parsing ==================== */

/* Parse query string into hashmap
 * Input: "key1=value1&key2=value2"
 * Output: hashmap with key-value pairs (strings)
 * Returns NULL on allocation failure
 * Caller must free with std_hashmap_free()
 */
std_hashmap_t* qs_parse(const char* query_string);

/* Parse with custom separators
 * sep: separator between key-value pairs (default: '&')
 * eq: separator between key and value (default: '=')
 * Returns NULL on allocation failure
 */
std_hashmap_t* qs_parse_custom(const char* query, char sep, char eq);

/* ==================== Query String Stringification ==================== */

/* Stringify hashmap to query string
 * Input: hashmap with key-value pairs
 * Output: "key1=value1&key2=value2"
 * Returns newly allocated string, caller must free()
 * Returns NULL on allocation failure
 */
char* qs_stringify(std_hashmap_t* params);

/* Stringify with custom separators
 * sep: separator between key-value pairs (default: '&')
 * eq: separator between key and value (default: '=')
 * Returns newly allocated string, caller must free()
 */
char* qs_stringify_custom(std_hashmap_t* params, char sep, char eq);

/* ==================== URL Encoding/Decoding ==================== */

/* URL encode a string (percent encoding per RFC 3986)
 * Input: "hello world" -> Output: "hello%20world"
 * Encodes: space, !, ", #, $, %, &, ', (, ), *, +, ,, /, :, ;, =, ?, @, [, ], {, }, |
 * Returns newly allocated string, caller must free()
 * Returns NULL on allocation failure
 */
char* qs_encode(const char* str);

/* URL decode a string (percent decoding)
 * Input: "hello%20world" -> Output: "hello world"
 * Returns newly allocated string, caller must free()
 * Returns NULL on allocation failure or invalid encoding
 */
char* qs_decode(const char* str);

#endif /* COSMORUN_QUERYSTRING_H */
