/**
 * cdp_http.h - HTTP API Server Module for CDP
 *
 * Provides HTTP REST API interface for CDP operations
 * Handles incoming HTTP requests and routes them to appropriate handlers
 */

#ifndef CDP_HTTP_H
#define CDP_HTTP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* HTTP server configuration */
typedef struct {
    int port;           // Port to listen on
    const char* host;   // Host to bind to (default: 127.0.0.1)
    int verbose;        // Verbose logging
} CDPHTTPConfig;

/* HTTP request structure */
typedef struct {
    char method[8];     // GET, POST, OPTIONS, etc.
    char path[1024];    // Request path
    char* body;         // Request body (if any)
    size_t body_len;    // Body length
    char* query;        // Query string (if any)
} CDPHTTPRequest;

/* HTTP response structure */
typedef struct {
    int status;         // HTTP status code
    const char* status_text;  // Status text (OK, Bad Request, etc.)
    const char* content_type; // Content-Type header
    char* body;         // Response body
    size_t body_len;    // Body length
} CDPHTTPResponse;

/* HTTP server functions */
int cdp_http_init(int port);
void cdp_http_cleanup(void);
int cdp_http_accept_connection(int listen_sock);
void cdp_http_handle_connection(int client_fd);

/* HTTP utility functions */
void cdp_http_send_response(int fd, int status, const char* status_text,
                            const char* content_type, const char* body);
void cdp_http_send_json(int fd, int status, const char* json);
void cdp_http_send_error(int fd, int status, const char* error_msg);
void cdp_http_send_cors_headers(int fd);

/* URL utilities */
void cdp_http_url_decode(const char* src, char* dest, size_t dest_size);
int cdp_http_parse_query(const char* query, char* param, size_t param_size, const char* key);
int cdp_http_parse_json_field(const char* json, char* value, size_t value_size, const char* field);

/* Request parsing */
int cdp_http_parse_request(const char* raw_request, CDPHTTPRequest* req);
int cdp_http_extract_body(const char* raw_request, char** body, size_t* body_len);

/* API endpoint handlers */
int cdp_http_handle_health(int client_fd, CDPHTTPRequest* req);
int cdp_http_handle_eval(int client_fd, CDPHTTPRequest* req);
int cdp_http_handle_api_cdp(int client_fd, CDPHTTPRequest* req);
int cdp_http_handle_logs(int client_fd, CDPHTTPRequest* req);
int cdp_http_handle_windows(int client_fd, CDPHTTPRequest* req);
int cdp_http_handle_window_activate(int client_fd, CDPHTTPRequest* req);
int cdp_http_handle_stats(int client_fd, CDPHTTPRequest* req);

/* Route registration and dispatch */
typedef int (*CDPHTTPHandler)(int client_fd, CDPHTTPRequest* req);

typedef struct {
    const char* method;
    const char* path;
    CDPHTTPHandler handler;
} CDPHTTPRoute;

int cdp_http_register_route(const char* method, const char* path, CDPHTTPHandler handler);
int cdp_http_dispatch_request(int client_fd, CDPHTTPRequest* req);

/* Global HTTP server state */
extern int cdp_http_listen_sock;
extern CDPHTTPConfig cdp_http_config;

#ifdef __cplusplus
}
#endif

#endif /* CDP_HTTP_H */