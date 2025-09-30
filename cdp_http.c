/**
 * cdp_http.c - HTTP API Server Implementation for CDP
 */

#include "cdp_http.h"
#include "cdp_internal.h"
#include "cdp_javascript.h"
#include "cdp_user_interface.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* Global HTTP server state */
int cdp_http_listen_sock = -1;
CDPHTTPConfig cdp_http_config = {
    .port = 0,
    .host = "127.0.0.1",
    .verbose = 0
};

/* External functions needed from cdp.c - will be moved to proper interface */
extern char* execute_cdp_cli_command(const char* command, const char* args);
extern char* execute_protocol_command(const char* url, const char* method);
extern int cdp_logs_tail(char *out, size_t out_sz, int max_lines, const char* filter);
extern CDPContext g_ctx;
extern int verbose;

/* Route table */
#define MAX_ROUTES 32
static CDPHTTPRoute route_table[MAX_ROUTES];
static int route_count = 0;

/* =============================
 * HTTP Server Core Functions
 * ============================= */

int cdp_http_init(int port) {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return -1;

    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (bind(s, (struct sockaddr*)&addr, sizeof(addr)) < 0 || listen(s, 5) < 0) {
        close(s);
        return -1;
    }

    cdp_http_listen_sock = s;
    cdp_http_config.port = port;
    return s;
}

void cdp_http_cleanup(void) {
    if (cdp_http_listen_sock >= 0) {
        close(cdp_http_listen_sock);
        cdp_http_listen_sock = -1;
    }
}

int cdp_http_accept_connection(int listen_sock) {
    struct sockaddr_in cliaddr;
    socklen_t cl = sizeof(cliaddr);
    return accept(listen_sock, (struct sockaddr*)&cliaddr, &cl);
}

/* =============================
 * HTTP Response Functions
 * ============================= */

void cdp_http_send_response(int fd, int status, const char* status_text,
                            const char* content_type, const char* body) {
    char header[1024];
    size_t blen = body ? strlen(body) : 0;

    snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET,POST,OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "\r\n",
        status, status_text, content_type, blen);

    send(fd, header, strlen(header), 0);
    if (body && blen > 0) send(fd, body, blen, 0);
}

void cdp_http_send_json(int fd, int status, const char* json) {
    cdp_http_send_response(fd, status, status == 200 ? "OK" : "Error",
                           "application/json", json);
}

void cdp_http_send_error(int fd, int status, const char* error_msg) {
    char json[1024];
    cdp_js_build_error_response(error_msg, NULL, json, sizeof(json));
    cdp_http_send_json(fd, status, json);
}

void cdp_http_send_cors_headers(int fd) {
    char hdr[1024];
    snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 204 No Content\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET,POST,OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Content-Length: 0\r\n\r\n");
    send(fd, hdr, strlen(hdr), 0);
}

/* =============================
 * URL and JSON Utilities
 * ============================= */

void cdp_http_url_decode(const char* src, char* dest, size_t dest_size) {
    size_t i = 0, j = 0;

    while (src[i] && j < dest_size - 1) {
        if (src[i] == '%' && src[i+1] && src[i+2]) {
            // Decode %XX
            char hex[3] = {src[i+1], src[i+2], '\0'};
            dest[j++] = (char)strtol(hex, NULL, 16);
            i += 3;
        } else if (src[i] == '+') {
            dest[j++] = ' ';
            i++;
        } else {
            dest[j++] = src[i++];
        }
    }
    dest[j] = '\0';
}

int cdp_http_parse_query(const char* query, char* param, size_t param_size, const char* key) {
    if (!query || !key) return -1;

    char search[256];
    snprintf(search, sizeof(search), "%s=", key);

    const char* start = strstr(query, search);
    if (!start) return -1;

    start += strlen(search);
    const char* end = strchr(start, '&');

    size_t len = end ? (size_t)(end - start) : strlen(start);
    if (len >= param_size) len = param_size - 1;

    strncpy(param, start, len);
    param[len] = '\0';

    // URL decode the parameter
    char decoded[1024];
    cdp_http_url_decode(param, decoded, sizeof(decoded));
    strncpy(param, decoded, param_size);

    return 0;
}

int cdp_http_parse_json_field(const char* json, char* value, size_t value_size, const char* field) {
    if (!json || !field) return -1;

    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", field);

    const char* p = strstr(json, search);
    if (!p) return -1;

    p = strchr(p, '"');
    if (p) p = strchr(p+1, '"');
    if (p && *(p+1) == ':') {
        p += 2;
        while (*p == ' ' || *p == '\t') p++;

        if (*p == '"') {
            // String value
            p++;
            const char* e = strchr(p, '"');
            if (e) {
                size_t len = e - p;
                if (len >= value_size) len = value_size - 1;
                strncpy(value, p, len);
                value[len] = '\0';
                return 0;
            }
        } else {
            // Number or boolean
            const char* e = p;
            while (*e && *e != ',' && *e != '}' && *e != ' ') e++;
            size_t len = e - p;
            if (len >= value_size) len = value_size - 1;
            strncpy(value, p, len);
            value[len] = '\0';
            return 0;
        }
    }

    return -1;
}

/* =============================
 * Request Parsing
 * ============================= */

int cdp_http_parse_request(const char* raw_request, CDPHTTPRequest* req) {
    if (!raw_request || !req) return -1;

    memset(req, 0, sizeof(CDPHTTPRequest));

    // Parse method and path
    sscanf(raw_request, "%7s %1023s", req->method, req->path);

    // Extract query string
    char* query_start = strchr(req->path, '?');
    if (query_start) {
        *query_start = '\0';
        req->query = query_start + 1;
    }

    // Extract body
    const char* body_start = strstr(raw_request, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        req->body_len = strlen(body_start);
        if (req->body_len > 0) {
            req->body = (char*)body_start;
        }
    }

    return 0;
}

int cdp_http_extract_body(const char* raw_request, char** body, size_t* body_len) {
    const char* body_start = strstr(raw_request, "\r\n\r\n");
    if (body_start) {
        body_start += 4;
        *body_len = strlen(body_start);
        if (*body_len > 0) {
            *body = (char*)body_start;
            return 0;
        }
    }
    *body = NULL;
    *body_len = 0;
    return -1;
}

/* =============================
 * API Endpoint Handlers
 * ============================= */

int cdp_http_handle_health(int client_fd, CDPHTTPRequest* req) {
    cdp_http_send_response(client_fd, 200, "OK", "text/plain; charset=utf-8", "ok");
    return 0;
}

int cdp_http_handle_eval(int client_fd, CDPHTTPRequest* req) {
    char cmd[4096] = "";

    // Try to get command from query string or body
    if (req->query) {
        cdp_http_parse_query(req->query, cmd, sizeof(cmd), "cmd");
    } else if (req->body) {
        // Try JSON parsing first
        if (cdp_http_parse_json_field(req->body, cmd, sizeof(cmd), "cmd") != 0) {
            // Fall back to raw body
            size_t len = strlen(req->body);
            if (len >= sizeof(cmd)) len = sizeof(cmd) - 1;
            strncpy(cmd, req->body, len);
            cmd[len] = '\0';
        }
    }

    if (*cmd) {
        char result[8192];
        if (cdp_runtime_eval(cmd, 1, 0, result, sizeof(result), g_ctx.config.timeout_ms) == 0) {
            cdp_http_send_response(client_fd, 200, "OK", "application/json", result);
        } else {
            cdp_http_send_error(client_fd, 500, "Evaluation failed");
        }
    } else {
        cdp_http_send_error(client_fd, 400, "Missing cmd parameter");
    }

    return 0;
}

int cdp_http_handle_api_cdp(int client_fd, CDPHTTPRequest* req) {
    if (strcmp(req->method, "POST") != 0) {
        cdp_http_send_error(client_fd, 405, "Method not allowed");
        return -1;
    }

    char command[256] = "";

    if (req->body) {
        cdp_http_parse_json_field(req->body, command, sizeof(command), "command");
    }

    if (*command) {
        char* result = execute_cdp_cli_command(command, "cli");
        if (result) {
            cdp_http_send_response(client_fd, 200, "OK", "application/json", result);
            free(result);
        } else {
            cdp_http_send_error(client_fd, 500, "Command execution failed");
        }
    } else {
        cdp_http_send_error(client_fd, 400, "Missing command");
    }

    return 0;
}

int cdp_http_handle_logs(int client_fd, CDPHTTPRequest* req) {
    int nlines = 50;

    if (req->query) {
        char tail_str[32];
        if (cdp_http_parse_query(req->query, tail_str, sizeof(tail_str), "tail") == 0) {
            nlines = atoi(tail_str);
            if (nlines <= 0) nlines = 50;
        }
    }

    char lines[8192];
    lines[0] = '\0';
    cdp_logs_tail(lines, sizeof(lines), nlines, NULL);

    // Build JSON array
    CDPJSONBuilder builder;
    cdp_js_builder_init(&builder);

    // Manual JSON array building for now
    char json[16384];
    size_t off = 0;
    off += snprintf(json + off, sizeof(json) - off, "{\"lines\":[");

    const char* p = lines;
    int first = 1;
    while (*p) {
        const char* nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);

        if (!first) off += snprintf(json + off, sizeof(json) - off, ",");
        off += snprintf(json + off, sizeof(json) - off, "\"");

        // Escape the line
        for (size_t i = 0; i < len && off < sizeof(json) - 10; i++) {
            if (p[i] == '"') off += snprintf(json + off, sizeof(json) - off, "\\\"");
            else if (p[i] == '\\') off += snprintf(json + off, sizeof(json) - off, "\\\\");
            else json[off++] = p[i];
        }
        off += snprintf(json + off, sizeof(json) - off, "\"");

        first = 0;
        p = nl ? nl + 1 : p + len;
        if (!*p) break;
    }
    off += snprintf(json + off, sizeof(json) - off, "]}");

    cdp_http_send_response(client_fd, 200, "OK", "application/json", json);
    return 0;
}

int cdp_http_handle_windows(int client_fd, CDPHTTPRequest* req) {
    char resp[65536];

    if (cdp_call_cmd("Target.getTargets", "{}", resp, sizeof(resp), g_ctx.config.timeout_ms) == 0) {
        cdp_http_send_response(client_fd, 200, "OK", "application/json", resp);
    } else {
        cdp_http_send_error(client_fd, 500, "Targets unavailable");
    }

    return 0;
}

int cdp_http_handle_window_activate(int client_fd, CDPHTTPRequest* req) {
    char tid[256] = "";

    // Try to get targetId from query or body
    if (req->query) {
        cdp_http_parse_query(req->query, tid, sizeof(tid), "targetId");
    }

    if (!*tid && req->body) {
        cdp_http_parse_json_field(req->body, tid, sizeof(tid), "targetId");
    }

    if (!*tid) {
        cdp_http_send_error(client_fd, 400, "Missing targetId");
        return -1;
    }

    char params[256];
    CDPJSONBuilder builder;
    cdp_js_builder_init(&builder);
    cdp_js_builder_add_string(&builder, "targetId", tid);
    strncpy(params, cdp_js_builder_get(&builder), sizeof(params));

    int rc = cdp_send_cmd("Target.activateTarget", params) >= 0 ? 0 : -1;

    if (rc == 0) {
        cdp_http_send_json(client_fd, 200, "{\"ok\":true}");
    } else {
        cdp_http_send_json(client_fd, 500, "{\"ok\":false}");
    }

    return rc;
}

int cdp_http_handle_stats(int client_fd, CDPHTTPRequest* req) {
    char stats[4096];

    CDPJSONBuilder builder;
    cdp_js_builder_init(&builder);
    cdp_js_builder_add_string(&builder, "status", "ok");
    cdp_js_builder_add_int(&builder, "uptime", 0);  // TODO: implement uptime tracking
    cdp_js_builder_add_int(&builder, "requests", 0); // TODO: implement request counting

    strncpy(stats, cdp_js_builder_get(&builder), sizeof(stats));
    cdp_http_send_json(client_fd, 200, stats);

    return 0;
}

/* =============================
 * Request Routing
 * ============================= */

int cdp_http_register_route(const char* method, const char* path, CDPHTTPHandler handler) {
    if (route_count >= MAX_ROUTES) return -1;

    route_table[route_count].method = method;
    route_table[route_count].path = path;
    route_table[route_count].handler = handler;
    route_count++;

    return 0;
}

int cdp_http_dispatch_request(int client_fd, CDPHTTPRequest* req) {
    // Look for matching route
    for (int i = 0; i < route_count; i++) {
        if (strcmp(req->method, route_table[i].method) == 0 &&
            strcmp(req->path, route_table[i].path) == 0) {
            return route_table[i].handler(client_fd, req);
        }
    }

    // No route found
    cdp_http_send_error(client_fd, 404, "Not found");
    return -1;
}

/* =============================
 * Main Connection Handler
 * ============================= */

void cdp_http_handle_connection(int client_fd) {
    char buf[8192];
    int n = recv(client_fd, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return;
    buf[n] = '\0';

    CDPHTTPRequest req;
    if (cdp_http_parse_request(buf, &req) != 0) {
        cdp_http_send_error(client_fd, 400, "Bad request");
        return;
    }

    // Handle CORS preflight
    if (strcmp(req.method, "OPTIONS") == 0) {
        cdp_http_send_cors_headers(client_fd);
        return;
    }

    // Built-in routes (for backward compatibility)
    if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/health") == 0) {
        cdp_http_handle_health(client_fd, &req);
        return;
    }

    if (strcmp(req.path, "/eval") == 0) {
        cdp_http_handle_eval(client_fd, &req);
        return;
    }

    if (strcmp(req.method, "POST") == 0 && strcmp(req.path, "/api/cdp") == 0) {
        cdp_http_handle_api_cdp(client_fd, &req);
        return;
    }

    if (strcmp(req.method, "GET") == 0 && strncmp(req.path, "/logs", 5) == 0) {
        cdp_http_handle_logs(client_fd, &req);
        return;
    }

    if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/windows") == 0) {
        cdp_http_handle_windows(client_fd, &req);
        return;
    }

    if (strcmp(req.method, "POST") == 0 && strncmp(req.path, "/windows/activate", 17) == 0) {
        cdp_http_handle_window_activate(client_fd, &req);
        return;
    }

    if (strcmp(req.method, "GET") == 0 && strcmp(req.path, "/stats") == 0) {
        cdp_http_handle_stats(client_fd, &req);
        return;
    }

    // Try registered routes
    cdp_http_dispatch_request(client_fd, &req);
}