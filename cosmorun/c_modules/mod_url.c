/*
 * mod_url.c - URL parsing and manipulation implementation
 */

#include "mod_url.h"
#include <ctype.h>

/* ==================== Module Initialization ==================== */

/* Static linking to mod_std functions (avoid deadlock from __import) */
extern std_string_t* std_string_new(const char *initial);
extern void std_string_free(std_string_t *str);
extern void std_string_append(std_string_t *str, const char *data);
extern void std_string_append_char(std_string_t *str, char c);
extern void std_string_clear(std_string_t *str);
extern const char* std_string_cstr(std_string_t *str);
extern size_t std_string_len(std_string_t *str);
extern std_vector_t* std_vector_new(void);
extern void std_vector_free(std_vector_t *vec);
extern void std_vector_push(std_vector_t *vec, void *item);
extern void* std_vector_pop(std_vector_t *vec);
extern void* std_vector_get(std_vector_t *vec, size_t index);
extern size_t std_vector_len(std_vector_t *vec);
extern std_hashmap_t* std_hashmap_new(void);
extern void std_hashmap_free(std_hashmap_t *map);
extern void std_hashmap_set(std_hashmap_t *map, const char *key, void *value);
extern void* std_hashmap_get(std_hashmap_t *map, const char *key);
extern void std_hashmap_remove(std_hashmap_t *map, const char *key);
extern size_t std_hashmap_size(std_hashmap_t *map);
extern void std_hashmap_foreach(std_hashmap_t *map, void (*fn)(const char*, void*, void*), void *userdata);

/* Auto-called by __import() when module loads */
int mod_url_init(void) {
    /* No dynamic imports needed - using static linking */
    return 0;
}

/* ==================== Helper Functions ==================== */

static int is_hex_digit(char c) {
    return (c >= '0' && c <= '9') ||
           (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

static int hex_to_int(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

static char* str_dup(const char *str) {
    if (!str) return NULL;
    size_t len = shim_strlen(str);
    char *dup = shim_malloc(len + 1);
    if (dup) {
        shim_memcpy(dup, str, len + 1);
    }
    return dup;
}

/* ==================== Encoding/Decoding ==================== */

char* url_encode(const char *str) {
    if (!str) return NULL;

    std_string_t *result = std_string_new("");
    if (!result) return NULL;

    for (const char *p = str; *p; p++) {
        if (isalnum(*p) || *p == '-' || *p == '_' || *p == '.' || *p == '~') {
            std_string_append_char(result, *p);
        } else if (*p == ' ') {
            std_string_append_char(result, '+');
        } else {
            char hex[4];
            shim_snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)*p);
            std_string_append(result, hex);
        }
    }

    char *encoded = str_dup(std_string_cstr(result));
    std_string_free(result);
    return encoded;
}

char* url_decode(const char *str) {
    if (!str) return NULL;

    std_string_t *result = std_string_new("");
    if (!result) return NULL;

    for (const char *p = str; *p; p++) {
        if (*p == '%' && is_hex_digit(p[1]) && is_hex_digit(p[2])) {
            char decoded = (hex_to_int(p[1]) << 4) | hex_to_int(p[2]);
            std_string_append_char(result, decoded);
            p += 2;
        } else if (*p == '+') {
            std_string_append_char(result, ' ');
        } else {
            std_string_append_char(result, *p);
        }
    }

    char *decoded = str_dup(std_string_cstr(result));
    std_string_free(result);
    return decoded;
}

/* ==================== Path Normalization ==================== */

char* url_normalize_path(const char *path) {
    if (!path) return NULL;

    std_vector_t *segments = std_vector_new();
    std_string_t *segment = std_string_new("");

    /* Split path into segments */
    for (const char *p = path; *p; p++) {
        if (*p == '/') {
            if (std_string_len(segment) > 0) {
                std_vector_push(segments, str_dup(std_string_cstr(segment)));
                std_string_clear(segment);
            }
        } else {
            std_string_append_char(segment, *p);
        }
    }
    if (std_string_len(segment) > 0) {
        std_vector_push(segments, str_dup(std_string_cstr(segment)));
    }
    std_string_free(segment);

    /* Normalize segments */
    std_vector_t *normalized = std_vector_new();
    for (size_t i = 0; i < std_vector_len(segments); i++) {
        char *seg = (char*)std_vector_get(segments, i);
        if (shim_strcmp(seg, ".") == 0) {
            /* Skip */
        } else if (shim_strcmp(seg, "..") == 0) {
            if (std_vector_len(normalized) > 0) {
                char *last = (char*)std_vector_pop(normalized);
                shim_free(last);
            }
        } else {
            std_vector_push(normalized, str_dup(seg));
        }
    }

    /* Build normalized path */
    std_string_t *result = std_string_new("");
    for (size_t i = 0; i < std_vector_len(normalized); i++) {
        std_string_append(result, "/");
        std_string_append(result, (char*)std_vector_get(normalized, i));
    }
    if (std_vector_len(normalized) == 0) {
        std_string_append(result, "/");
    }

    /* Cleanup */
    for (size_t i = 0; i < std_vector_len(segments); i++) {
        shim_free(std_vector_get(segments, i));
    }
    std_vector_free(segments);

    for (size_t i = 0; i < std_vector_len(normalized); i++) {
        shim_free(std_vector_get(normalized, i));
    }
    std_vector_free(normalized);

    char *normalized_path = str_dup(std_string_cstr(result));
    std_string_free(result);
    return normalized_path;
}

/* ==================== Protocol Functions ==================== */

int url_is_supported_protocol(const char *protocol) {
    if (!protocol) return 0;
    return shim_strcmp(protocol, "http:") == 0 ||
           shim_strcmp(protocol, "https:") == 0 ||
           shim_strcmp(protocol, "file:") == 0 ||
           shim_strcmp(protocol, "ftp:") == 0 ||
           shim_strcmp(protocol, "ws:") == 0 ||
           shim_strcmp(protocol, "wss:") == 0;
}

int url_get_default_port(const char *protocol) {
    if (!protocol) return -1;
    if (shim_strcmp(protocol, "http:") == 0 || shim_strcmp(protocol, "ws:") == 0) return 80;
    if (shim_strcmp(protocol, "https:") == 0 || shim_strcmp(protocol, "wss:") == 0) return 443;
    if (shim_strcmp(protocol, "ftp:") == 0) return 21;
    return -1;
}

char* url_extract_protocol(const char *url_string) {
    if (!url_string) return NULL;

    const char *colon = shim_strchr(url_string, ':');
    if (!colon) return NULL;

    size_t len = colon - url_string + 1;
    char *protocol = shim_malloc(len + 1);
    if (protocol) {
        shim_memcpy(protocol, url_string, len);
        protocol[len] = '\0';
    }
    return protocol;
}

int url_is_absolute(const char *url_string) {
    if (!url_string) return 0;
    const char *colon = shim_strchr(url_string, ':');
    return colon != NULL && colon[1] == '/' && colon[2] == '/';
}

/* ==================== Hostname Validation ==================== */

int url_is_ipv6(const char *hostname) {
    if (!hostname) return 0;
    return hostname[0] == '[' && shim_strchr(hostname, ']') != NULL;
}

int url_validate_hostname(const char *hostname) {
    if (!hostname || shim_strlen(hostname) == 0) return 0;

    /* IPv6 addresses */
    if (url_is_ipv6(hostname)) {
        /* Simple validation: check for [ and ] */
        return shim_strchr(hostname, ']') != NULL;
    }

    /* IPv4 and domain names - basic validation */
    return 1;
}

/* ==================== Query Parameter Functions ==================== */

/* Helper context for query string building */
typedef struct {
    std_string_t *str;
    int *first;
} query_ctx_t;

/* Helper callback for building query string */
static void append_param_callback(const char *key, void *value, void *userdata) {
    query_ctx_t *c = (query_ctx_t*)userdata;
    if (!*c->first) {
        std_string_append(c->str, "&");
    }
    *c->first = 0;

    char *encoded_key = url_encode(key);
    char *encoded_value = url_encode((char*)value);
    std_string_append(c->str, encoded_key);
    std_string_append(c->str, "=");
    std_string_append(c->str, encoded_value);
    shim_free(encoded_key);
    shim_free(encoded_value);
}

/* Helper callback for freeing query param values */
static void free_value_callback(const char *key, void *value, void *userdata) {
    (void)key;
    (void)userdata;
    shim_free(value);
}

static void parse_query_params(url_t *url) {
    if (!url->query || shim_strlen(url->query) == 0) return;

    char *query_copy = str_dup(url->query);
    if (!query_copy) return;

    /* Manual parsing to avoid strtok_r */
    char *p = query_copy;
    char *pair_start = p;

    while (*p) {
        if (*p == '&' || *(p + 1) == '\0') {
            /* Found end of pair */
            int is_last = (*(p + 1) == '\0' && *p != '&');
            if (!is_last) *p = '\0';

            char *equals = shim_strchr(pair_start, '=');
            if (equals) {
                *equals = '\0';
                char *key = url_decode(pair_start);
                char *value = url_decode(equals + 1);
                if (key && value) {
                    std_hashmap_set(url->query_params, key, str_dup(value));
                }
                shim_free(key);
                shim_free(value);
            } else {
                char *key = url_decode(pair_start);
                if (key) {
                    std_hashmap_set(url->query_params, key, str_dup(""));
                    shim_free(key);
                }
            }

            if (!is_last) {
                pair_start = p + 1;
            }
        }
        p++;
    }

    shim_free(query_copy);
}

const char* url_get_query_param(url_t *url, const char *key) {
    if (!url || !key) return NULL;
    return (const char*)std_hashmap_get(url->query_params, key);
}

void url_set_query_param(url_t *url, const char *key, const char *value) {
    if (!url || !key) return;

    char *value_copy = str_dup(value ? value : "");
    if (!value_copy) return;

    /* Free old value if exists */
    char *old = (char*)std_hashmap_get(url->query_params, key);
    if (old) shim_free(old);

    std_hashmap_set(url->query_params, key, value_copy);
}

void url_remove_query_param(url_t *url, const char *key) {
    if (!url || !key) return;

    char *old = (char*)std_hashmap_get(url->query_params, key);
    if (old) {
        shim_free(old);
        std_hashmap_remove(url->query_params, key);
    }
}

char* url_build_query_string(url_t *url) {
    if (!url || std_hashmap_size(url->query_params) == 0) {
        return str_dup("");
    }

    std_string_t *query = std_string_new("?");
    int first = 1;

    query_ctx_t ctx = { query, &first };

    std_hashmap_foreach(url->query_params, append_param_callback, &ctx);

    char *result = str_dup(std_string_cstr(query));
    std_string_free(query);
    return result;
}

/* ==================== URL Parsing ==================== */

url_t* url_parse(const char *url_string) {
    if (!url_string) return NULL;

    url_t *url = shim_calloc(1, sizeof(url_t));
    if (!url) return NULL;

    url->query_params = std_hashmap_new();
    url->href = str_dup(url_string);

    const char *p = url_string;

    /* Parse protocol */
    const char *colon = shim_strchr(p, ':');
    if (colon && colon[1] == '/' && colon[2] == '/') {
        url->protocol = shim_malloc(colon - p + 2);
        shim_memcpy(url->protocol, p, colon - p + 1);
        url->protocol[colon - p + 1] = '\0';
        p = colon + 3; /* Skip :// */
    } else if (colon) {
        /* file: or other protocol without // */
        url->protocol = shim_malloc(colon - p + 2);
        shim_memcpy(url->protocol, p, colon - p + 1);
        url->protocol[colon - p + 1] = '\0';
        p = colon + 1;
    }

    /* Parse authentication */
    const char *at = shim_strchr(p, '@');
    const char *slash = shim_strchr(p, '/');
    if (at && (!slash || at < slash)) {
        const char *colon_auth = shim_strchr(p, ':');
        if (colon_auth && colon_auth < at) {
            url->username = shim_malloc(colon_auth - p + 1);
            shim_memcpy(url->username, p, colon_auth - p);
            url->username[colon_auth - p] = '\0';

            url->password = shim_malloc(at - colon_auth);
            shim_memcpy(url->password, colon_auth + 1, at - colon_auth - 1);
            url->password[at - colon_auth - 1] = '\0';
        } else {
            url->username = shim_malloc(at - p + 1);
            shim_memcpy(url->username, p, at - p);
            url->username[at - p] = '\0';
        }

        url->auth = shim_malloc(at - p + 1);
        shim_memcpy(url->auth, p, at - p);
        url->auth[at - p] = '\0';

        p = at + 1;
    }

    /* Parse host and hostname */
    slash = shim_strchr(p, '/');
    const char *question = shim_strchr(p, '?');
    const char *hash_mark = shim_strchr(p, '#');
    const char *host_end = slash;
    if (!host_end || (question && question < host_end)) host_end = question;
    if (!host_end || (hash_mark && hash_mark < host_end)) host_end = hash_mark;
    if (!host_end) host_end = p + shim_strlen(p);

    if (host_end > p) {
        url->host = shim_malloc(host_end - p + 1);
        shim_memcpy(url->host, p, host_end - p);
        url->host[host_end - p] = '\0';

        /* Parse port from host */
        if (url_is_ipv6(url->host)) {
            /* IPv6: [::1]:8080 */
            const char *bracket_close = shim_strchr(url->host, ']');
            if (bracket_close && bracket_close[1] == ':') {
                url->hostname = shim_malloc(bracket_close - url->host + 2);
                shim_memcpy(url->hostname, url->host, bracket_close - url->host + 1);
                url->hostname[bracket_close - url->host + 1] = '\0';
                url->port = str_dup(bracket_close + 2);
            } else {
                url->hostname = str_dup(url->host);
            }
        } else {
            /* Regular hostname */
            const char *port_colon = shim_strchr(url->host, ':');
            if (port_colon) {
                url->hostname = shim_malloc(port_colon - url->host + 1);
                shim_memcpy(url->hostname, url->host, port_colon - url->host);
                url->hostname[port_colon - url->host] = '\0';
                url->port = str_dup(port_colon + 1);
            } else {
                url->hostname = str_dup(url->host);
            }
        }

        p = host_end;
    }

    /* Parse pathname */
    if (*p == '/') {
        question = shim_strchr(p, '?');
        hash_mark = shim_strchr(p, '#');
        const char *path_end = question;
        if (!path_end || (hash_mark && hash_mark < path_end)) path_end = hash_mark;
        if (!path_end) path_end = p + shim_strlen(p);

        url->pathname = shim_malloc(path_end - p + 1);
        shim_memcpy(url->pathname, p, path_end - p);
        url->pathname[path_end - p] = '\0';

        p = path_end;
    } else if (!url->pathname) {
        url->pathname = str_dup("/");
    }

    /* Parse query */
    if (*p == '?') {
        hash_mark = shim_strchr(p, '#');
        const char *query_end = hash_mark ? hash_mark : p + shim_strlen(p);

        url->search = shim_malloc(query_end - p + 1);
        shim_memcpy(url->search, p, query_end - p);
        url->search[query_end - p] = '\0';

        url->query = shim_malloc(query_end - p);
        shim_memcpy(url->query, p + 1, query_end - p - 1);
        url->query[query_end - p - 1] = '\0';

        parse_query_params(url);

        p = query_end;
    }

    /* Parse hash */
    if (*p == '#') {
        url->hash = str_dup(p);
    }

    return url;
}

/* ==================== URL Formatting ==================== */

char* url_format(url_t *url) {
    if (!url) return NULL;

    std_string_t *result = std_string_new("");

    if (url->protocol) {
        std_string_append(result, url->protocol);
        if (shim_strcmp(url->protocol, "file:") != 0) {
            std_string_append(result, "//");
        }
    }

    if (url->auth) {
        std_string_append(result, url->auth);
        std_string_append(result, "@");
    }

    if (url->host) {
        std_string_append(result, url->host);
    }

    if (url->pathname) {
        std_string_append(result, url->pathname);
    }

    if (std_hashmap_size(url->query_params) > 0) {
        char *query_str = url_build_query_string(url);
        std_string_append(result, query_str);
        shim_free(query_str);
    } else if (url->search) {
        std_string_append(result, url->search);
    }

    if (url->hash) {
        std_string_append(result, url->hash);
    }

    char *formatted = str_dup(std_string_cstr(result));
    std_string_free(result);
    return formatted;
}

/* ==================== URL Resolution ==================== */

char* url_resolve(const char *base, const char *relative) {
    if (!base || !relative) return NULL;

    /* If relative is absolute, return it */
    if (url_is_absolute(relative)) {
        return str_dup(relative);
    }

    url_t *base_url = url_parse(base);
    if (!base_url) return NULL;

    url_t *result = shim_calloc(1, sizeof(url_t));
    result->query_params = std_hashmap_new();

    /* Copy protocol and host from base */
    if (base_url->protocol) result->protocol = str_dup(base_url->protocol);
    if (base_url->host) result->host = str_dup(base_url->host);
    if (base_url->hostname) result->hostname = str_dup(base_url->hostname);
    if (base_url->port) result->port = str_dup(base_url->port);

    /* Handle relative path */
    if (relative[0] == '/') {
        /* Absolute path */
        result->pathname = str_dup(relative);
    } else {
        /* Relative path */
        std_string_t *path = std_string_new("");
        if (base_url->pathname) {
            const char *last_slash = shim_strrchr(base_url->pathname, '/');
            if (last_slash) {
                std_string_append(path, base_url->pathname);
                ((char*)std_string_cstr(path))[last_slash - base_url->pathname + 1] = '\0';
                path->len = last_slash - base_url->pathname + 1;
            }
        }
        std_string_append(path, relative);
        result->pathname = url_normalize_path(std_string_cstr(path));
        std_string_free(path);
    }

    char *resolved = url_format(result);
    url_free(base_url);
    url_free(result);

    return resolved;
}

/* ==================== URL Cleanup ==================== */

void url_free(url_t *url) {
    if (!url) return;

    shim_free(url->href);
    shim_free(url->protocol);
    shim_free(url->auth);
    shim_free(url->username);
    shim_free(url->password);
    shim_free(url->host);
    shim_free(url->hostname);
    shim_free(url->port);
    shim_free(url->pathname);
    shim_free(url->search);
    shim_free(url->query);
    shim_free(url->hash);

    /* Free query params hashmap values */
    if (url->query_params) {
        std_hashmap_foreach(url->query_params, free_value_callback, NULL);
        std_hashmap_free(url->query_params);
    }

    shim_free(url);
}
