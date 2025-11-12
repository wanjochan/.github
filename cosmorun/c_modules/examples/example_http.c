/*
 * example_http.c - HTTP Client/Server Examples
 *
 * Demonstrates:
 * - Simple GET/POST requests
 * - Custom headers and query parameters
 * - Basic HTTP server
 * - Error handling
 */

#include "src/cosmo_libc.h"
#include "c_modules/mod_http.c"

void example_http_client() {
    printf("\n========================================\n");
    printf("  HTTP Client Examples\n");
    printf("========================================\n\n");

    /* Example 1: Simple GET request */
    printf("1. Simple GET Request:\n");
    http_response_t *resp = http_get("http://httpbin.org/get");

    if (resp) {
        printf("   Status: %d %s\n", resp->status_code, resp->status_message);
        const char *body_str = resp->body ? std_string_cstr(resp->body) : NULL;
        printf("   Body length: %zu bytes\n", body_str ? strlen(body_str) : 0);
        if (body_str && strlen(body_str) < 500) {
            printf("   Body: %s\n", body_str);
        }
        http_response_free(resp);
    } else {
        fprintf(stderr, "   ERROR: Request failed\n");
    }
    printf("\n");

    /* Example 2: POST request with JSON data */
    printf("2. POST Request with JSON:\n");
    const char *json_data = "{\"name\":\"Alice\",\"age\":30}";
    resp = http_post("http://httpbin.org/post", json_data, "application/json");

    if (resp) {
        printf("   Status: %d %s\n", resp->status_code, resp->status_message);
        printf("   Posted data: %s\n", json_data);
        http_response_free(resp);
    } else {
        fprintf(stderr, "   ERROR: POST failed\n");
    }
    printf("\n");

    /* Example 3: Request with custom headers */
    printf("3. GET Request with Custom Headers:\n");
    std_hashmap_t *headers = std_hashmap_new();
    std_hashmap_set(headers, "User-Agent", "CosmoRun/1.0");
    std_hashmap_set(headers, "Accept", "application/json");

    resp = http_request("GET", "http://httpbin.org/headers", headers, NULL);

    if (resp) {
        printf("   Status: %d\n", resp->status_code);
        printf("   Custom headers sent successfully\n");
        http_response_free(resp);
    }
    std_hashmap_free(headers);
    printf("\n");
}

void example_http_server_handler(http_request_t *req, http_response_t *resp) {
    printf("   Received: %s %s\n", req->method, req->path);

    /* Handle different routes */
    if (strcmp(req->path, "/") == 0) {
        http_response_set_status(resp, 200);
        http_response_set_body(resp, "<h1>Welcome to CosmoRun HTTP Server</h1>");
        http_response_set_header(resp, "Content-Type", "text/html");
    }
    else if (strcmp(req->path, "/api/status") == 0) {
        http_response_set_status(resp, 200);
        http_response_set_body(resp, "{\"status\":\"ok\",\"server\":\"cosmorun\"}");
        http_response_set_header(resp, "Content-Type", "application/json");
    }
    else {
        http_response_set_status(resp, 404);
        http_response_set_body(resp, "{\"error\":\"Not Found\"}");
        http_response_set_header(resp, "Content-Type", "application/json");
    }
}

void example_http_server() {
    printf("\n========================================\n");
    printf("  HTTP Server Example\n");
    printf("========================================\n\n");

    printf("Creating HTTP server on port 8080...\n");
    printf("(Server example - would block, skipping actual run)\n");
    printf("Usage:\n");
    printf("  http_server_t *server = http_server_create(8080, example_http_server_handler);\n");
    printf("  http_server_run(server);  // Blocks until stopped\n");
    printf("  http_server_free(server);\n");
    printf("\n");
}

int main(void) {
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║  CosmoRun HTTP Module Examples        ║\n");
    printf("╚════════════════════════════════════════╝\n");

    example_http_client();
    example_http_server();

    printf("========================================\n");
    printf("  All HTTP examples completed!\n");
    printf("========================================\n\n");

    return 0;
}
