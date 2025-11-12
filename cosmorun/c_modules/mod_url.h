#ifndef COSMORUN_URL_H
#define COSMORUN_URL_H

/*
 * mod_url - Node.js-style URL parsing and manipulation for cosmorun
 *
 * Provides URL parsing similar to Node.js URL module:
 * - Parse URL strings into structured components
 * - Format URL structures back to strings
 * - Query parameter manipulation
 * - Relative URL resolution
 * - Percent encoding/decoding
 * - IPv6 address support
 */

#include "system/libc_shim/sys_libc_shim.h"
#include "mod_std.h"

/* ==================== URL Structure ==================== */

typedef struct {
    char *href;          /* Full URL string */
    char *protocol;      /* Protocol (http:, https:, etc.) */
    char *auth;          /* Authentication (user:pass) */
    char *username;      /* Username */
    char *password;      /* Password */
    char *host;          /* Full host (hostname:port) */
    char *hostname;      /* Hostname without port */
    char *port;          /* Port number as string */
    char *pathname;      /* Path component */
    char *search;        /* Query string with leading ? */
    char *query;         /* Query string without leading ? */
    char *hash;          /* Fragment with leading # */
    std_hashmap_t *query_params;  /* Parsed query parameters */
} url_t;

/* ==================== URL Creation and Parsing ==================== */

/* Parse URL string into url_t structure
 * Returns NULL if URL is invalid or malformed
 * Caller must free with url_free()
 */
url_t* url_parse(const char *url_string);

/* Format url_t structure back to string
 * Returns newly allocated string, caller must free()
 */
char* url_format(url_t *url);

/* Resolve relative URL against base URL
 * Returns newly allocated absolute URL string, caller must free()
 * Returns NULL on error
 */
char* url_resolve(const char *base, const char *relative);

/* Free url_t structure and all its components */
void url_free(url_t *url);

/* ==================== Query Parameter Manipulation ==================== */

/* Get query parameter by key
 * Returns NULL if key doesn't exist
 * Returned string is owned by url_t, do not free
 */
const char* url_get_query_param(url_t *url, const char *key);

/* Set query parameter
 * Creates or updates parameter, handles percent encoding
 */
void url_set_query_param(url_t *url, const char *key, const char *value);

/* Remove query parameter by key */
void url_remove_query_param(url_t *url, const char *key);

/* Build query string from parameters
 * Returns newly allocated string with leading ?, caller must free()
 */
char* url_build_query_string(url_t *url);

/* ==================== Encoding/Decoding ==================== */

/* Percent-encode string for URL usage
 * Returns newly allocated string, caller must free()
 */
char* url_encode(const char *str);

/* Percent-decode URL-encoded string
 * Returns newly allocated string, caller must free()
 * Returns NULL on invalid encoding
 */
char* url_decode(const char *str);

/* ==================== Utility Functions ==================== */

/* Normalize path (resolve . and .. components)
 * Returns newly allocated normalized path, caller must free()
 */
char* url_normalize_path(const char *path);

/* Check if URL is absolute (has protocol) */
int url_is_absolute(const char *url_string);

/* Extract protocol from URL string
 * Returns newly allocated protocol string (e.g., "http:"), caller must free()
 * Returns NULL if no protocol found
 */
char* url_extract_protocol(const char *url_string);

/* Validate hostname (including IPv6)
 * Returns 1 if valid, 0 if invalid
 */
int url_validate_hostname(const char *hostname);

/* Check if hostname is IPv6 address (enclosed in [])
 * Returns 1 if IPv6, 0 otherwise
 */
int url_is_ipv6(const char *hostname);

/* ==================== Supported Protocols ==================== */

/* Protocol validation
 * Returns 1 if protocol is supported, 0 otherwise
 * Supported: http:, https:, file:, ftp:, ws:, wss:
 */
int url_is_supported_protocol(const char *protocol);

/* Default ports for protocols */
int url_get_default_port(const char *protocol);

#endif /* COSMORUN_URL_H */
