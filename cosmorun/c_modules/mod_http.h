#ifndef MOD_HTTP_H
#define MOD_HTTP_H

/*
 * mod_http - HTTP client and server for cosmorun
 *
 * Provides HTTP/1.1 functionality:
 * - HTTP client (GET, POST, custom requests)
 * - HTTP server (request handling, routing)
 * - Header parsing and building
 * - Query parameter parsing
 * - Status code handling
 *
 * Dependencies: mod_net.c, mod_std.h
 * Platform support: Linux (primary), macOS, Windows (future)
 */

#include "../cosmorun_system/libc_shim/sys_libc_shim.h"
#include "mod_std.h"
#include "mod_net.h"

/* ==================== Constants ==================== */

/* HTTP Methods */
#define HTTP_METHOD_GET     "GET"
#define HTTP_METHOD_POST    "POST"
#define HTTP_METHOD_PUT     "PUT"
#define HTTP_METHOD_DELETE  "DELETE"
#define HTTP_METHOD_HEAD    "HEAD"
#define HTTP_METHOD_OPTIONS "OPTIONS"
#define HTTP_METHOD_PATCH   "PATCH"

/* HTTP Versions */
#define HTTP_VERSION_10     "HTTP/1.0"
#define HTTP_VERSION_11     "HTTP/1.1"

/* HTTP Status Codes */
#define HTTP_STATUS_OK                  200
#define HTTP_STATUS_CREATED             201
#define HTTP_STATUS_NO_CONTENT          204
#define HTTP_STATUS_MOVED_PERMANENTLY   301
#define HTTP_STATUS_FOUND               302
#define HTTP_STATUS_NOT_MODIFIED        304
#define HTTP_STATUS_BAD_REQUEST         400
#define HTTP_STATUS_UNAUTHORIZED        401
#define HTTP_STATUS_FORBIDDEN           403
#define HTTP_STATUS_NOT_FOUND           404
#define HTTP_STATUS_METHOD_NOT_ALLOWED  405
#define HTTP_STATUS_INTERNAL_ERROR      500
#define HTTP_STATUS_NOT_IMPLEMENTED     501
#define HTTP_STATUS_BAD_GATEWAY         502
#define HTTP_STATUS_SERVICE_UNAVAILABLE 503

/* Error Codes */
#define HTTP_ERR_NONE              0
#define HTTP_ERR_INVALID_URL      -1
#define HTTP_ERR_CONNECT          -2
#define HTTP_ERR_SEND             -3
#define HTTP_ERR_RECV             -4
#define HTTP_ERR_PARSE            -5
#define HTTP_ERR_TIMEOUT          -6
#define HTTP_ERR_MEMORY           -7
#define HTTP_ERR_INVALID_RESPONSE -8

/* Default values */
#define HTTP_DEFAULT_PORT         80
#define HTTP_DEFAULT_TIMEOUT_MS   30000
#define HTTP_MAX_HEADER_SIZE      8192
#define HTTP_MAX_URL_SIZE         2048

/* ==================== Data Structures ==================== */

/* HTTP URL structure */
typedef struct {
    char *scheme;       /* "http" or "https" */
    char *host;         /* hostname or IP address */
    int port;           /* port number (default 80) */
    char *path;         /* request path (default "/") */
    char *query;        /* query string (optional) */
    char *fragment;     /* fragment identifier (optional) */
} http_url_t;

/* HTTP request structure */
typedef struct {
    char *method;              /* HTTP method (GET, POST, etc.) */
    char *path;                /* Request path */
    char *version;             /* HTTP version (default HTTP/1.1) */
    std_hashmap_t *headers;    /* Request headers */
    std_hashmap_t *query_params; /* Query parameters (parsed from path) */
    std_string_t *body;        /* Request body */
    net_socket_t *socket;      /* Client socket (server-side) */
} http_request_t;

/* HTTP response structure */
typedef struct {
    int status_code;           /* HTTP status code */
    char *status_message;      /* Status message */
    char *version;             /* HTTP version */
    std_hashmap_t *headers;    /* Response headers */
    std_string_t *body;        /* Response body */
    std_error_t *error;        /* Last error (if any) */
} http_response_t;

/* HTTP server handler callback */
typedef void (*http_handler_fn)(http_request_t *req, http_response_t *resp);

/* HTTP server structure */
typedef struct {
    net_socket_t *listen_sock; /* Listening socket */
    int port;                  /* Server port */
    http_handler_fn handler;   /* Request handler callback */
    int running;               /* Server running flag */
    std_error_t *error;        /* Last error */
} http_server_t;

/* ==================== Module Initialization ==================== */

/*
 * Initialize HTTP module and load dependencies
 * This function is called automatically by the runtime
 * Returns: 0 on success, -1 on failure
 */
int http_init(void);

/* ==================== URL Parsing ==================== */

/*
 * Parse URL string into components
 * Args:
 *   url - URL string (e.g., "http://example.com:8080/path?query=value")
 * Returns: parsed URL structure, NULL on failure
 */
http_url_t* http_url_parse(const char *url);

/*
 * Free URL structure
 */
void http_url_free(http_url_t *url);

/* ==================== HTTP Client ==================== */

/*
 * Perform HTTP GET request
 * Args:
 *   url - full URL string
 * Returns: HTTP response, NULL on failure
 */
http_response_t* http_get(const char *url);

/*
 * Perform HTTP POST request
 * Args:
 *   url - full URL string
 *   data - request body data
 *   content_type - Content-Type header (e.g., "application/json")
 * Returns: HTTP response, NULL on failure
 */
http_response_t* http_post(const char *url, const char *data, const char *content_type);

/*
 * Perform custom HTTP request
 * Args:
 *   method - HTTP method (GET, POST, etc.)
 *   url - full URL string
 *   headers - custom headers (can be NULL)
 *   body - request body (can be NULL)
 * Returns: HTTP response, NULL on failure
 */
http_response_t* http_request(const char *method, const char *url,
                               std_hashmap_t *headers, const char *body);

/*
 * Free HTTP response
 */
void http_response_free(http_response_t *resp);

/* ==================== HTTP Server ==================== */

/*
 * Create HTTP server
 * Args:
 *   port - port to listen on (0 = any available port)
 *   handler - request handler callback
 * Returns: server instance, NULL on failure
 */
http_server_t* http_server_create(int port, http_handler_fn handler);

/*
 * Start HTTP server (blocking)
 * This function runs the server loop until stopped
 * Returns: 0 on success, negative on error
 */
int http_server_run(http_server_t *server);

/*
 * Stop HTTP server
 */
void http_server_stop(http_server_t *server);

/*
 * Free HTTP server
 */
void http_server_free(http_server_t *server);

/* ==================== Request/Response Helpers ==================== */

/*
 * Create new HTTP request
 * Returns: request structure, NULL on failure
 */
http_request_t* http_request_new(void);

/*
 * Free HTTP request
 */
void http_request_free(http_request_t *req);

/*
 * Create new HTTP response
 * Returns: response structure, NULL on failure
 */
http_response_t* http_response_new(void);

/*
 * Set response status code
 */
void http_response_set_status(http_response_t *resp, int status_code);

/*
 * Set response header
 */
void http_response_set_header(http_response_t *resp, const char *name, const char *value);

/*
 * Set response body
 */
void http_response_set_body(http_response_t *resp, const char *body);

/*
 * Send response to client (server-side)
 * Args:
 *   resp - response to send
 *   sock - client socket
 * Returns: 0 on success, negative on failure
 */
int http_response_send(http_response_t *resp, net_socket_t *sock);

/* ==================== Header Utilities ==================== */

/*
 * Get header value from request
 * Returns: header value or NULL if not found
 */
const char* http_request_get_header(http_request_t *req, const char *name);

/*
 * Get header value from response
 * Returns: header value or NULL if not found
 */
const char* http_response_get_header(http_response_t *resp, const char *name);

/*
 * Parse query string into parameters
 * Args:
 *   query - query string (e.g., "key1=value1&key2=value2")
 * Returns: hashmap of parameters, NULL on failure
 */
std_hashmap_t* http_parse_query(const char *query);

/*
 * Get query parameter from request
 * Returns: parameter value or NULL if not found
 */
const char* http_request_get_param(http_request_t *req, const char *name);

/* ==================== Status Code Helpers ==================== */

/*
 * Get status message for HTTP status code
 * Returns: status message string (e.g., "OK", "Not Found")
 */
const char* http_status_message(int status_code);

/* ==================== Utility Functions ==================== */

/*
 * URL-encode string
 * Returns: encoded string (must be freed by caller)
 */
char* http_url_encode(const char *str);

/*
 * URL-decode string
 * Returns: decoded string (must be freed by caller)
 */
char* http_url_decode(const char *str);

#endif /* MOD_HTTP_H */
