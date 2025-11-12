# mod_http - HTTP Client/Server Module

**File**: `c_modules/mod_http.c`
**Dependencies**: `mod_net.c`, `mod_std.c`
**External libs**: None (pure cosmorun implementation)

## Overview

`mod_http` provides a complete HTTP/1.1 client and server implementation built on top of `mod_net`. It supports GET/POST requests, custom headers, query parameters, and basic HTTP server functionality.

## Quick Start

```c
#include "mod_http.c"

// Simple GET request
http_response_t *resp = http_get("http://example.com/api");
if (resp) {
    printf("Status: %d\n", resp->status_code);
    printf("Body: %s\n", resp->body);
    http_response_free(resp);
}
```

## Common Use Cases

### 1. GET Request with Custom Headers

```c
// Create headers
std_hashmap_t *headers = std_hashmap_new();
std_hashmap_set(headers, "Authorization", "Bearer YOUR_TOKEN");
std_hashmap_set(headers, "Accept", "application/json");

// Make request
http_response_t *resp = http_request("GET", "http://api.example.com/data",
                                     headers, NULL);

if (resp) {
    printf("Response: %s\n", resp->body);
    http_response_free(resp);
}
std_hashmap_free(headers);
```

### 2. POST JSON Data

```c
const char *json = "{\"name\":\"Alice\",\"email\":\"alice@example.com\"}";
http_response_t *resp = http_post("http://api.example.com/users",
                                  json, "application/json");

if (resp) {
    if (resp->status_code == 201) {
        printf("User created successfully\n");
    }
    http_response_free(resp);
}
```

### 3. Simple HTTP Server

```c
void handler(http_request_t *req, http_response_t *resp) {
    if (strcmp(req->path, "/") == 0) {
        http_response_set_status(resp, 200);
        http_response_set_body(resp, "<h1>Hello World</h1>");
        http_response_set_header(resp, "Content-Type", "text/html");
    } else {
        http_response_set_status(resp, 404);
        http_response_set_body(resp, "Not Found");
    }
}

http_server_t *server = http_server_create(8080, handler);
http_server_run(server);  // Blocks until stopped
http_server_free(server);
```

### 4. Parse Query Parameters

```c
// Request: GET /search?q=hello&limit=10
const char *query = http_request_get_param(req, "q");      // "hello"
const char *limit = http_request_get_param(req, "limit");  // "10"
```

## API Reference

### Client Functions

#### `http_response_t* http_get(const char *url)`
Perform HTTP GET request.
- **Returns**: Response object or NULL on error
- **Must free**: `http_response_free(resp)`

#### `http_response_t* http_post(const char *url, const char *data, const char *content_type)`
Perform HTTP POST request with body.
- **data**: Request body content
- **content_type**: MIME type (e.g., "application/json")

#### `http_response_t* http_request(const char *method, const char *url, std_hashmap_t *headers, const char *body)`
Generic HTTP request with full control.
- **method**: "GET", "POST", "PUT", "DELETE", etc.
- **headers**: Optional custom headers (can be NULL)
- **body**: Optional request body (can be NULL)

### Server Functions

#### `http_server_t* http_server_create(int port, http_handler_fn handler)`
Create HTTP server listening on port.
- **handler**: Callback function `void handler(http_request_t*, http_response_t*)`

#### `int http_server_run(http_server_t *server)`
Start server (blocks until stopped).
- **Returns**: 0 on success, -1 on error

#### `void http_server_stop(http_server_t *server)`
Stop running server (call from signal handler).

#### `void http_server_free(http_server_t *server)`
Free server resources.

### Response Functions

#### `const char* http_response_get_header(http_response_t *resp, const char *name)`
Get response header value (case-insensitive).

#### `void http_response_free(http_response_t *resp)`
Free response object and all associated memory.

### Request Functions

#### `const char* http_request_get_header(http_request_t *req, const char *name)`
Get request header value (case-insensitive).

#### `const char* http_request_get_param(http_request_t *req, const char *name)`
Get URL query parameter value.

### Utility Functions

#### `char* http_url_encode(const char *str)`
URL-encode a string (e.g., "hello world" → "hello%20world").
- **Must free** the returned string

#### `char* http_url_decode(const char *str)`
URL-decode a string (e.g., "hello%20world" → "hello world").
- **Must free** the returned string

## Data Structures

```c
typedef struct {
    int status_code;           // HTTP status (200, 404, etc.)
    char *status_message;      // "OK", "Not Found", etc.
    std_hashmap_t *headers;    // Response headers
    char *body;                // Response body
} http_response_t;

typedef struct {
    char *method;              // "GET", "POST", etc.
    char *path;                // "/api/users"
    char *version;             // "HTTP/1.1"
    std_hashmap_t *headers;    // Request headers
    std_hashmap_t *query_params;  // Parsed query string
    char *body;                // Request body
} http_request_t;
```

## Error Handling

### Check for NULL Response

```c
http_response_t *resp = http_get(url);
if (!resp) {
    fprintf(stderr, "HTTP request failed\n");
    return -1;
}
```

### Check Status Code

```c
if (resp->status_code >= 400) {
    fprintf(stderr, "HTTP error: %d %s\n",
            resp->status_code, resp->status_message);
}
```

### Graceful Degradation

```c
const char *content_type = http_response_get_header(resp, "Content-Type");
if (content_type && strstr(content_type, "application/json")) {
    // Parse JSON response
} else {
    // Handle as plain text
}
```

## Performance Tips

1. **Reuse connections**: For multiple requests to same host, consider keeping the socket open (advanced usage)

2. **Set timeouts**: Default timeout is 30 seconds. Adjust based on your needs by configuring the underlying `net_socket_t`

3. **Buffer sizes**: Default read buffer is 4KB. For large responses, consider streaming or increasing buffer size

4. **Keep-Alive**: Server supports HTTP/1.1 persistent connections by default

5. **Error recovery**: Always check return values and free resources even on error

## Common Pitfalls

- **Memory leaks**: Always call `http_response_free()` for responses
- **NULL checks**: Check for NULL before accessing response fields
- **Header names**: Header lookups are case-insensitive ("Content-Type" == "content-type")
- **URL encoding**: Remember to URL-encode special characters in URLs
- **Server blocking**: `http_server_run()` blocks - use threads or separate process for production

## Example Output

```
========================================
  HTTP Client Examples
========================================

1. Simple GET Request:
   Status: 200 OK
   Body length: 1256 bytes

2. POST Request with JSON:
   Status: 201 Created
   Posted data: {"name":"Alice","age":30}

✓ All HTTP examples completed!
```

## Related Modules

- **mod_net**: Underlying TCP socket implementation
- **mod_std**: String and hashmap utilities
- **mod_json**: Often used with HTTP for API communication
- **mod_curl**: Alternative HTTP client using libcurl

## See Also

- Full example: `c_modules/examples/example_http.c`
- HTTP/1.1 spec: RFC 2616
- URL encoding: RFC 3986
