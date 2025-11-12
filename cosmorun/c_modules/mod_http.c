/*
 * mod_http - HTTP client and server for cosmorun
 *
 * Implementation of HTTP/1.1 protocol on top of mod_net.c
 * Migrated to System Layer API (libc_shim)
 */

#include "src/cosmorun.h"
#include "../cosmorun_system/libc_shim/sys_libc_shim.h"
#include "mod_http.h"

/* Include error handling implementation (header-only) */
#include "mod_error_impl.h"

/* ==================== Module Initialization ==================== */

/* Static linking to mod_std functions (avoid deadlock from __import) */
extern std_string_t* std_string_new(const char *initial);
extern void std_string_free(std_string_t *str);
extern void std_string_clear(std_string_t *str);
extern void std_string_append(std_string_t *str, const char *data);
extern void std_string_append_char(std_string_t *str, char c);
extern const char* std_string_cstr(std_string_t *str);
extern std_hashmap_t* std_hashmap_new(void);
extern void std_hashmap_free(std_hashmap_t *map);
extern void std_hashmap_set(std_hashmap_t *map, const char *key, void *value);
extern void* std_hashmap_get(std_hashmap_t *map, const char *key);
extern int std_hashmap_has(std_hashmap_t *map, const char *key);
extern size_t std_hashmap_size(std_hashmap_t *map);
extern void std_hashmap_foreach(std_hashmap_t *map, void (*fn)(const char*, void*, void*), void *userdata);
extern void std_error_free(std_error_t *err);

/* Static linking to mod_net functions (avoid deadlock from __import) */
extern net_socket_t* net_tcp_connect(const char *host, int port);
extern net_socket_t* net_tcp_listen(int port, int backlog);
extern net_socket_t* net_tcp_accept(net_socket_t *server);
extern int net_send_all(net_socket_t *sock, const void *data, size_t len);
extern int net_recv(net_socket_t *sock, void *buf, size_t len);
extern int net_recv_all(net_socket_t *sock, void *buf, size_t len);
extern void net_socket_close(net_socket_t *sock);

/* Auto-called by __import() when module loads */
/* Returns: 0 on success, -1 on failure */
int mod_http_init(void) {
    /* No dynamic imports needed - using static linking */
    return 0;
}

/* ==================== Internal Helpers ==================== */

/* Case-insensitive string comparison */
static int strcasecmp_impl(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        char c1 = (*s1 >= 'A' && *s1 <= 'Z') ? *s1 + 32 : *s1;
        char c2 = (*s2 >= 'A' && *s2 <= 'Z') ? *s2 + 32 : *s2;
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

/* Trim whitespace from string */
static char* trim_whitespace(char *str) {
    char *end;
    while (*str == ' ' || *str == '\t' || *str == '\r' || *str == '\n') str++;
    if (*str == 0) return str;
    end = str + shim_strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) end--;
    *(end + 1) = 0;
    return str;
}

/* ==================== Status Messages ==================== */

const char* http_status_message(int status_code) {
    switch (status_code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 304: return "Not Modified";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        default: return "Unknown";
    }
}

/* ==================== URL Parsing ==================== */

http_url_t* http_url_parse(const char *url) {
    if (!url) return NULL;

    http_url_t *parsed = (http_url_t*)shim_malloc(sizeof(http_url_t));
    if (!parsed) return NULL;

    shim_memset(parsed, 0, sizeof(http_url_t));

    /* Parse scheme */
    const char *p = shim_strstr(url, "://");
    if (p) {
        size_t scheme_len = p - url;
        parsed->scheme = (char*)shim_malloc(scheme_len + 1);
        if (!parsed->scheme) goto error;
        shim_strncpy(parsed->scheme, url, scheme_len);
        parsed->scheme[scheme_len] = 0;
        url = p + 3; /* Skip "://" */
    } else {
        parsed->scheme = shim_strdup("http");
        if (!parsed->scheme) goto error;
    }

    /* Parse host and port */
    char host_buf[256];
    const char *path_start = shim_strchr(url, '/');
    const char *query_start = shim_strchr(url, '?');
    const char *host_end = path_start ? path_start : (query_start ? query_start : url + shim_strlen(url));

    size_t host_len = host_end - url;
    if (host_len >= sizeof(host_buf)) goto error;
    shim_strncpy(host_buf, url, host_len);
    host_buf[host_len] = 0;

    /* Check for port */
    char *colon = shim_strchr(host_buf, ':');
    if (colon) {
        *colon = 0;
        parsed->port = atoi(colon + 1);
        parsed->host = shim_strdup(host_buf);
    } else {
        parsed->host = shim_strdup(host_buf);
        parsed->port = HTTP_DEFAULT_PORT;
    }

    if (!parsed->host) goto error;

    /* Parse path */
    if (path_start) {
        const char *path_end = query_start ? query_start : path_start + shim_strlen(path_start);
        size_t path_len = path_end - path_start;
        parsed->path = (char*)shim_malloc(path_len + 1);
        if (!parsed->path) goto error;
        shim_strncpy(parsed->path, path_start, path_len);
        parsed->path[path_len] = 0;
    } else {
        parsed->path = shim_strdup("/");
        if (!parsed->path) goto error;
    }

    /* Parse query */
    if (query_start) {
        const char *fragment = shim_strchr(query_start, '#');
        const char *query_end = fragment ? fragment : query_start + shim_strlen(query_start);
        size_t query_len = query_end - query_start - 1; /* Skip '?' */
        parsed->query = (char*)shim_malloc(query_len + 1);
        if (!parsed->query) goto error;
        shim_strncpy(parsed->query, query_start + 1, query_len);
        parsed->query[query_len] = 0;

        /* Parse fragment */
        if (fragment) {
            parsed->fragment = shim_strdup(fragment + 1);
        }
    }

    return parsed;

error:
    http_url_free(parsed);
    return NULL;
}

void http_url_free(http_url_t *url) {
    if (!url) return;
    shim_free(url->scheme);
    shim_free(url->host);
    shim_free(url->path);
    shim_free(url->query);
    shim_free(url->fragment);
    shim_free(url);
}

/* ==================== Request/Response Management ==================== */

http_request_t* http_request_new(void) {
    http_request_t *req = (http_request_t*)shim_malloc(sizeof(http_request_t));
    if (!req) return NULL;

    shim_memset(req, 0, sizeof(http_request_t));
    req->method = shim_strdup(HTTP_METHOD_GET);
    req->path = shim_strdup("/");
    req->version = shim_strdup(HTTP_VERSION_11);
    req->headers = std_hashmap_new();
    req->query_params = std_hashmap_new();
    req->body = std_string_new("");

    if (!req->method || !req->path || !req->version ||
        !req->headers || !req->query_params || !req->body) {
        http_request_free(req);
        return NULL;
    }

    return req;
}

void http_request_free(http_request_t *req) {
    if (!req) return;
    shim_free(req->method);
    shim_free(req->path);
    shim_free(req->version);
    if (req->headers) std_hashmap_free(req->headers);
    if (req->query_params) std_hashmap_free(req->query_params);
    if (req->body) std_string_free(req->body);
    shim_free(req);
}

http_response_t* http_response_new(void) {
    http_response_t *resp = (http_response_t*)shim_malloc(sizeof(http_response_t));
    if (!resp) return NULL;

    shim_memset(resp, 0, sizeof(http_response_t));
    resp->status_code = HTTP_STATUS_OK;
    resp->status_message = shim_strdup(http_status_message(HTTP_STATUS_OK));
    resp->version = shim_strdup(HTTP_VERSION_11);
    resp->headers = std_hashmap_new();
    resp->body = std_string_new("");

    if (!resp->status_message || !resp->version || !resp->headers || !resp->body) {
        http_response_free(resp);
        return NULL;
    }

    return resp;
}

void http_response_free(http_response_t *resp) {
    if (!resp) return;
    shim_free(resp->status_message);
    shim_free(resp->version);
    if (resp->headers) std_hashmap_free(resp->headers);
    if (resp->body) std_string_free(resp->body);
    if (resp->error) std_error_free(resp->error);
    shim_free(resp);
}

void http_response_set_status(http_response_t *resp, int status_code) {
    if (!resp) return;
    resp->status_code = status_code;
    shim_free(resp->status_message);
    resp->status_message = shim_strdup(http_status_message(status_code));
}

void http_response_set_header(http_response_t *resp, const char *name, const char *value) {
    if (!resp || !name || !value) return;
    std_hashmap_set(resp->headers, name, (void*)shim_strdup(value));
}

void http_response_set_body(http_response_t *resp, const char *body) {
    if (!resp || !body) return;
    std_string_clear(resp->body);
    std_string_append(resp->body, body);
}

/* ==================== Header Utilities ==================== */

const char* http_request_get_header(http_request_t *req, const char *name) {
    if (!req || !name) return NULL;
    return (const char*)std_hashmap_get(req->headers, name);
}

const char* http_response_get_header(http_response_t *resp, const char *name) {
    if (!resp || !name) return NULL;
    return (const char*)std_hashmap_get(resp->headers, name);
}

const char* http_request_get_param(http_request_t *req, const char *name) {
    if (!req || !name) return NULL;
    return (const char*)std_hashmap_get(req->query_params, name);
}

/* ==================== Query Parsing ==================== */

std_hashmap_t* http_parse_query(const char *query) {
    if (!query) return NULL;

    std_hashmap_t *params = std_hashmap_new();
    if (!params) return NULL;

    char *query_copy = shim_strdup(query);
    if (!query_copy) {
        std_hashmap_free(params);
        return NULL;
    }

    char *pair = strtok(query_copy, "&");
    while (pair) {
        char *eq = shim_strchr(pair, '=');
        if (eq) {
            *eq = 0;
            char *key = http_url_decode(pair);
            char *value = http_url_decode(eq + 1);
            if (key && value) {
                std_hashmap_set(params, key, value);
            }
            shim_free(key);
            /* value will be freed when hashmap is freed */
        }
        pair = strtok(NULL, "&");
    }

    shim_free(query_copy);
    return params;
}

/* ==================== URL Encoding ==================== */

static int is_url_safe(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
}

char* http_url_encode(const char *str) {
    if (!str) return NULL;

    size_t len = shim_strlen(str);
    char *encoded = (char*)shim_malloc(len * 3 + 1); /* Worst case: all chars encoded */
    if (!encoded) return NULL;

    char *p = encoded;
    for (size_t i = 0; i < len; i++) {
        if (is_url_safe(str[i])) {
            *p++ = str[i];
        } else if (str[i] == ' ') {
            *p++ = '+';
        } else {
            shim_sprintf(p, "%%%02X", (unsigned char)str[i]);
            p += 3;
        }
    }
    *p = 0;

    return encoded;
}

char* http_url_decode(const char *str) {
    if (!str) return NULL;

    size_t len = shim_strlen(str);
    char *decoded = (char*)shim_malloc(len + 1);
    if (!decoded) return NULL;

    char *p = decoded;
    for (size_t i = 0; i < len; i++) {
        if (str[i] == '+') {
            *p++ = ' ';
        } else if (str[i] == '%' && i + 2 < len) {
            int value;
            sscanf(str + i + 1, "%2x", &value);
            *p++ = (char)value;
            i += 2;
        } else {
            *p++ = str[i];
        }
    }
    *p = 0;

    return decoded;
}

/* ==================== HTTP Client ==================== */

static int http_send_request(net_socket_t *sock, const char *method, const char *path,
                              std_hashmap_t *headers, const char *body) {
    std_string_t *request = std_string_new("");
    if (!request) return -1;

    /* Request line */
    std_string_append(request, method);
    std_string_append(request, " ");
    std_string_append(request, path);
    std_string_append(request, " HTTP/1.1\r\n");

    /* Headers */
    if (headers) {
        /* Add headers from hashmap */
        /* Note: This is simplified - should iterate hashmap */
    }

    /* Content-Length for body */
    if (body) {
        char content_len[32];
        shim_sprintf(content_len, "Content-Length: %zu\r\n", shim_strlen(body));
        std_string_append(request, content_len);
    }

    /* End of headers */
    std_string_append(request, "\r\n");

    /* Body */
    if (body) {
        std_string_append(request, body);
    }

    /* Send request */
    const char *req_str = std_string_cstr(request);
    int result = net_send_all(sock, req_str, shim_strlen(req_str));

    std_string_free(request);
    return result;
}

static http_response_t* http_recv_response(net_socket_t *sock) {
    http_response_t *resp = http_response_new();
    if (!resp) return NULL;

    /* Read response headers */
    char header_buf[HTTP_MAX_HEADER_SIZE];
    size_t header_len = 0;
    int headers_done = 0;

    while (header_len < HTTP_MAX_HEADER_SIZE - 1 && !headers_done) {
        char c;
        int n = net_recv(sock, &c, 1);
        if (n <= 0) {
            http_response_free(resp);
            return NULL;
        }
        header_buf[header_len++] = c;

        /* Check for end of headers (\r\n\r\n) */
        if (header_len >= 4 &&
            header_buf[header_len-4] == '\r' && header_buf[header_len-3] == '\n' &&
            header_buf[header_len-2] == '\r' && header_buf[header_len-1] == '\n') {
            headers_done = 1;
        }
    }

    header_buf[header_len] = 0;

    /* Parse status line */
    char *line = strtok(header_buf, "\r\n");
    if (line) {
        char version[16];
        int status;
        char message[128];
        if (sscanf(line, "%15s %d %127[^\r\n]", version, &status, message) >= 2) {
            resp->status_code = status;
            shim_free(resp->status_message);
            resp->status_message = shim_strdup(message);
        }
    }

    /* Parse headers */
    while ((line = strtok(NULL, "\r\n")) != NULL) {
        char *colon = shim_strchr(line, ':');
        if (colon) {
            *colon = 0;
            char *name = trim_whitespace(line);
            char *value = trim_whitespace(colon + 1);
            std_hashmap_set(resp->headers, name, (void*)shim_strdup(value));
        }
    }

    /* Read body */
    const char *content_len_str = http_response_get_header(resp, "Content-Length");
    if (content_len_str) {
        size_t content_len = atoi(content_len_str);
        char *body_buf = (char*)shim_malloc(content_len + 1);
        if (body_buf) {
            if (net_recv_all(sock, body_buf, content_len) == 0) {
                body_buf[content_len] = 0;
                std_string_append(resp->body, body_buf);
            }
            shim_free(body_buf);
        }
    } else {
        /* Read until connection closes */
        char buf[1024];
        int n;
        while ((n = net_recv(sock, buf, sizeof(buf))) > 0) {
            for (int i = 0; i < n; i++) {
                std_string_append_char(resp->body, buf[i]);
            }
        }
    }

    return resp;
}

http_response_t* http_request(const char *method, const char *url,
                               std_hashmap_t *headers, const char *body) {
    http_url_t *parsed = http_url_parse(url);
    if (!parsed) return NULL;

    /* Connect to server */
    net_socket_t *sock = net_tcp_connect(parsed->host, parsed->port);
    if (!sock) {
        http_url_free(parsed);
        return NULL;
    }

    /* Build full path with query */
    char full_path[HTTP_MAX_URL_SIZE];
    if (parsed->query) {
        shim_snprintf(full_path, sizeof(full_path), "%s?%s", parsed->path, parsed->query);
    } else {
        shim_snprintf(full_path, sizeof(full_path), "%s", parsed->path);
    }

    /* Add Host header if not present */
    if (!headers) {
        headers = std_hashmap_new();
    }
    if (!std_hashmap_has(headers, "Host")) {
        std_hashmap_set(headers, "Host", (void*)shim_strdup(parsed->host));
    }
    if (!std_hashmap_has(headers, "Connection")) {
        std_hashmap_set(headers, "Connection", (void*)shim_strdup("close"));
    }

    /* Send request */
    if (http_send_request(sock, method, full_path, headers, body) < 0) {
        net_socket_close(sock);
        http_url_free(parsed);
        return NULL;
    }

    /* Receive response */
    http_response_t *resp = http_recv_response(sock);

    net_socket_close(sock);
    http_url_free(parsed);

    return resp;
}

http_response_t* http_get(const char *url) {
    return http_request(HTTP_METHOD_GET, url, NULL, NULL);
}

http_response_t* http_post(const char *url, const char *data, const char *content_type) {
    std_hashmap_t *headers = std_hashmap_new();
    if (!headers) return NULL;

    if (content_type) {
        std_hashmap_set(headers, "Content-Type", (void*)shim_strdup(content_type));
    } else {
        std_hashmap_set(headers, "Content-Type", (void*)shim_strdup("application/x-www-form-urlencoded"));
    }

    http_response_t *resp = http_request(HTTP_METHOD_POST, url, headers, data);

    std_hashmap_free(headers);
    return resp;
}

/* ==================== HTTP Server ==================== */

static http_request_t* http_parse_request(net_socket_t *client) {
    http_request_t *req = http_request_new();
    if (!req) return NULL;

    req->socket = client;

    /* Read request headers */
    char header_buf[HTTP_MAX_HEADER_SIZE];
    size_t header_len = 0;
    int headers_done = 0;

    while (header_len < HTTP_MAX_HEADER_SIZE - 1 && !headers_done) {
        char c;
        int n = net_recv(client, &c, 1);
        if (n <= 0) {
            http_request_free(req);
            return NULL;
        }
        header_buf[header_len++] = c;

        /* Check for end of headers */
        if (header_len >= 4 &&
            header_buf[header_len-4] == '\r' && header_buf[header_len-3] == '\n' &&
            header_buf[header_len-2] == '\r' && header_buf[header_len-1] == '\n') {
            headers_done = 1;
        }
    }

    header_buf[header_len] = 0;

    /* Parse request line */
    char *line = strtok(header_buf, "\r\n");
    if (line) {
        char method[16], path[1024], version[16];
        if (sscanf(line, "%15s %1023s %15s", method, path, version) == 3) {
            shim_free(req->method);
            shim_free(req->path);
            shim_free(req->version);
            req->method = shim_strdup(method);
            req->path = shim_strdup(path);
            req->version = shim_strdup(version);

            /* Parse query parameters from path */
            char *query = shim_strchr(req->path, '?');
            if (query) {
                *query = 0;
                req->query_params = http_parse_query(query + 1);
            }
        }
    }

    /* Parse headers */
    while ((line = strtok(NULL, "\r\n")) != NULL) {
        char *colon = shim_strchr(line, ':');
        if (colon) {
            *colon = 0;
            char *name = trim_whitespace(line);
            char *value = trim_whitespace(colon + 1);
            std_hashmap_set(req->headers, name, (void*)shim_strdup(value));
        }
    }

    /* Read body if present */
    const char *content_len_str = http_request_get_header(req, "Content-Length");
    if (content_len_str) {
        size_t content_len = atoi(content_len_str);
        char *body_buf = (char*)shim_malloc(content_len + 1);
        if (body_buf) {
            if (net_recv_all(client, body_buf, content_len) == 0) {
                body_buf[content_len] = 0;
                std_string_append(req->body, body_buf);
            }
            shim_free(body_buf);
        }
    }

    return req;
}

/* Helper struct and callback for header iteration in http_response_send */
typedef struct {
    std_string_t *response;
} http_header_append_context_t;

static void http_append_header_callback(const char *key, void *value, void *userdata) {
    http_header_append_context_t *ctx = (http_header_append_context_t*)userdata;
    const char *val = (const char*)value;
    if (key && val) {
        std_string_append(ctx->response, key);
        std_string_append(ctx->response, ": ");
        std_string_append(ctx->response, val);
        std_string_append(ctx->response, "\r\n");
    }
}

int http_response_send(http_response_t *resp, net_socket_t *sock) {
    if (!resp || !sock) return -1;

    std_string_t *response = std_string_new("");
    if (!response) return -1;

    /* Status line */
    char status_line[256];
    shim_sprintf(status_line, "%s %d %s\r\n", resp->version, resp->status_code, resp->status_message);
    std_string_append(response, status_line);

    /* Headers */
    const char *body_str = std_string_cstr(resp->body);
    size_t body_len = shim_strlen(body_str);

    char content_len[64];
    shim_sprintf(content_len, "Content-Length: %zu\r\n", body_len);
    std_string_append(response, content_len);

    /* Iterate hashmap for custom headers */
    if (resp->headers && std_hashmap_size(resp->headers) > 0) {
        http_header_append_context_t hctx = { .response = response };
        std_hashmap_foreach(resp->headers, http_append_header_callback, &hctx);
    }

    /* End of headers */
    std_string_append(response, "\r\n");

    /* Body */
    if (body_len > 0) {
        std_string_append(response, body_str);
    }

    /* Send response */
    const char *resp_str = std_string_cstr(response);
    int result = net_send_all(sock, resp_str, shim_strlen(resp_str));

    std_string_free(response);
    return result;
}

http_server_t* http_server_create(int port, http_handler_fn handler) {
    if (!handler) return NULL;

    http_server_t *server = (http_server_t*)shim_malloc(sizeof(http_server_t));
    if (!server) return NULL;

    shim_memset(server, 0, sizeof(http_server_t));
    server->handler = handler;
    server->port = port;

    /* Create listening socket */
    server->listen_sock = net_tcp_listen(port, 5);
    if (!server->listen_sock) {
        shim_free(server);
        return NULL;
    }

    return server;
}

int http_server_run(http_server_t *server) {
    if (!server || !server->listen_sock) return -1;

    server->running = 1;

    while (server->running) {
        /* Accept client connection */
        net_socket_t *client = net_tcp_accept(server->listen_sock);
        if (!client) {
            if (!server->running) break; /* Server stopped */
            continue;
        }

        /* Parse request */
        http_request_t *req = http_parse_request(client);
        if (!req) {
            net_socket_close(client);
            continue;
        }

        /* Create response */
        http_response_t *resp = http_response_new();
        if (!resp) {
            http_request_free(req);
            net_socket_close(client);
            continue;
        }

        /* Call handler */
        server->handler(req, resp);

        /* Send response */
        http_response_send(resp, client);

        /* Cleanup */
        http_request_free(req);
        http_response_free(resp);
        net_socket_close(client);
    }

    return 0;
}

void http_server_stop(http_server_t *server) {
    if (!server) return;
    server->running = 0;
}

void http_server_free(http_server_t *server) {
    if (!server) return;
    if (server->listen_sock) net_socket_close(server->listen_sock);
    if (server->error) std_error_free(server->error);
    shim_free(server);
}
