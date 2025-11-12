#ifndef MOD_CURL_H
#define MOD_CURL_H

/*
 * mod_curl - libcurl HTTP client wrapper for cosmorun v2
 *
 * Provides HTTP client functionality through dynamic loading of libcurl:
 * - HTTP GET/POST requests
 * - Custom headers support
 * - File upload/download
 * - Timeout configuration
 * - Context-based handle management
 */

#include "../cosmorun_system/libc_shim/sys_libc_shim.h"
#include "mod_std.h"

/* CURL opaque types */
typedef void CURL;
typedef void curl_slist;

/* CURL result codes */
typedef enum {
    CURLE_OK = 0,
    CURLE_UNSUPPORTED_PROTOCOL = 1,
    CURLE_FAILED_INIT = 2,
    CURLE_URL_MALFORMAT = 3,
    CURLE_COULDNT_RESOLVE_HOST = 6,
    CURLE_COULDNT_CONNECT = 7,
    CURLE_HTTP_RETURNED_ERROR = 22,
    CURLE_WRITE_ERROR = 23,
    CURLE_OPERATION_TIMEDOUT = 28,
    CURLE_SSL_CONNECT_ERROR = 35,
    CURLE_TOO_MANY_REDIRECTS = 47,
    CURLE_GOT_NOTHING = 52,
    CURLE_SEND_ERROR = 55,
    CURLE_RECV_ERROR = 56
} CURLcode;

/* CURL options */
typedef enum {
    CURLOPT_URL = 10002,
    CURLOPT_WRITEFUNCTION = 20011,
    CURLOPT_WRITEDATA = 10001,
    CURLOPT_READFUNCTION = 20012,
    CURLOPT_READDATA = 10009,
    CURLOPT_TIMEOUT = 13,
    CURLOPT_CONNECTTIMEOUT = 78,
    CURLOPT_HTTPHEADER = 10023,
    CURLOPT_POST = 47,
    CURLOPT_POSTFIELDS = 10015,
    CURLOPT_POSTFIELDSIZE = 60,
    CURLOPT_UPLOAD = 46,
    CURLOPT_INFILESIZE_LARGE = 30115,
    CURLOPT_FOLLOWLOCATION = 52,
    CURLOPT_MAXREDIRS = 68,
    CURLOPT_USERAGENT = 10018,
    CURLOPT_HTTPGET = 80,
    CURLOPT_NOBODY = 44,
    CURLOPT_CUSTOMREQUEST = 10036,
    CURLOPT_VERBOSE = 41,
    CURLOPT_ERRORBUFFER = 10010,
    CURLOPT_NOPROXY = 10177,
    CURLOPT_SSL_VERIFYPEER = 64,
    CURLOPT_SSL_VERIFYHOST = 81
} CURLoption;

/* CURL info types */
typedef enum {
    CURLINFO_RESPONSE_CODE = 0x200000 + 2,
    CURLINFO_CONTENT_TYPE = 0x100000 + 18
} CURLINFO;

/* ==================== CURL Context ==================== */

typedef struct {
    void *lib_handle;         /* dlopen handle for libcurl.so */
    void *curl_handle;        /* CURL* easy handle */
    std_hashmap_t *headers;   /* Custom HTTP headers */
    long timeout;             /* Request timeout in seconds */
    long connect_timeout;     /* Connection timeout in seconds */
    char error_buffer[256];   /* Error message buffer */

    /* libcurl function pointers */
    void* (*curl_easy_init)(void);
    void* curl_easy_setopt_raw;       /* Original varargs function pointer (use via ffi) */
    void* curl_easy_getinfo_raw;      /* Original varargs function pointer (use via ffi) */
    CURLcode (*curl_easy_perform)(CURL *curl);
    void (*curl_easy_cleanup)(CURL *curl);
    const char* (*curl_easy_strerror)(CURLcode code);

    /* Header list functions */
    curl_slist* (*curl_slist_append)(curl_slist *list, const char *string);
    void (*curl_slist_free_all)(curl_slist *list);

    /* Global init/cleanup */
    CURLcode (*curl_global_init)(long flags);
    void (*curl_global_cleanup)(void);
} curl_context_t;

/* ==================== Public API ==================== */

/**
 * Initialize CURL context and load libcurl library
 * @param lib_path Optional library path (NULL for auto-detection)
 * @return Allocated context or NULL on failure
 */
curl_context_t* curl_init(const char *lib_path);

/**
 * Cleanup and free CURL context
 * @param ctx CURL context
 */
void curl_cleanup(curl_context_t *ctx);

/**
 * Set request timeout in seconds
 * @param ctx CURL context
 * @param timeout_seconds Timeout in seconds (0 = no timeout)
 */
void curl_set_timeout(curl_context_t *ctx, long timeout_seconds);

/**
 * Set connection timeout in seconds
 * @param ctx CURL context
 * @param timeout_seconds Connection timeout in seconds (0 = default)
 */
void curl_set_connect_timeout(curl_context_t *ctx, long timeout_seconds);

/**
 * Add custom HTTP header
 * @param ctx CURL context
 * @param key Header name
 * @param value Header value
 */
void curl_add_header(curl_context_t *ctx, const char *key, const char *value);

/**
 * Clear all custom headers
 * @param ctx CURL context
 */
void curl_clear_headers(curl_context_t *ctx);

/**
 * Perform HTTP GET request
 * @param ctx CURL context
 * @param url Target URL
 * @return Response body as std_string_t (caller must free), or NULL on error
 */
std_string_t* curl_get(curl_context_t *ctx, const char *url);

/**
 * Perform HTTP POST request
 * @param ctx CURL context
 * @param url Target URL
 * @param data POST data (application/x-www-form-urlencoded or raw)
 * @return Response body as std_string_t (caller must free), or NULL on error
 */
std_string_t* curl_post(curl_context_t *ctx, const char *url, const char *data);

/**
 * Perform HTTP POST request with custom content type
 * @param ctx CURL context
 * @param url Target URL
 * @param data POST data
 * @param content_type Content-Type header value (e.g., "application/json")
 * @return Response body as std_string_t (caller must free), or NULL on error
 */
std_string_t* curl_post_content_type(curl_context_t *ctx, const char *url,
                                      const char *data, const char *content_type);

/**
 * Download file from URL to local path
 * @param ctx CURL context
 * @param url Source URL
 * @param filepath Local file path to save
 * @return 0 on success, -1 on error
 */
int curl_download(curl_context_t *ctx, const char *url, const char *filepath);

/**
 * Upload file to URL via HTTP PUT
 * @param ctx CURL context
 * @param url Target URL
 * @param filepath Local file path to upload
 * @return Response body as std_string_t (caller must free), or NULL on error
 */
std_string_t* curl_upload(curl_context_t *ctx, const char *url, const char *filepath);

/**
 * Get HTTP response code from last request
 * @param ctx CURL context
 * @return HTTP status code (200, 404, etc.) or -1 on error
 */
long curl_get_response_code(curl_context_t *ctx);

/**
 * Get last error message
 * @param ctx CURL context
 * @return Error message string (valid until next request)
 */
const char* curl_get_error(curl_context_t *ctx);

/**
 * Self-test function to verify libcurl loading and basic functionality
 * @param lib_path Optional library path (NULL for auto-detection)
 * @return 0 on success, -1 on failure
 */
int curl_selftest(const char *lib_path);

/**
 * Self-test with auto library detection
 * @return 0 on success, -1 on failure
 */
int curl_selftest_default(void);

#endif /* MOD_CURL_H */
