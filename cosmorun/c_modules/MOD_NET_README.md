# mod_net - Network Socket Module

**File**: `c_modules/mod_net.c`
**Dependencies**: `mod_std.c`
**External libs**: System socket libraries (libc)

## Overview

`mod_net` provides cross-platform TCP socket networking with DNS resolution. It wraps system socket APIs with a clean, easy-to-use interface and proper error handling. Supports both client and server operations.

## Quick Start

```c
#include "mod_net.c"

// TCP client
net_socket_t *sock = net_tcp_connect("example.com", 80);
if (sock) {
    net_send_all(sock, "GET / HTTP/1.0\r\n\r\n", 18);

    char buffer[1024];
    int received = net_recv(sock, buffer, sizeof(buffer));

    net_socket_close(sock);
}
```

## Common Use Cases

### 1. Simple TCP Client

```c
// Connect to server
net_socket_t *sock = net_tcp_connect("api.example.com", 443);
if (!sock) {
    fprintf(stderr, "Connection failed\n");
    return -1;
}

// Send request
const char *request = "GET /api/status HTTP/1.1\r\nHost: api.example.com\r\n\r\n";
int sent = net_send_all(sock, request, strlen(request));

if (sent < 0) {
    fprintf(stderr, "Send failed\n");
    net_socket_close(sock);
    return -1;
}

// Receive response
char buffer[4096];
int received = net_recv(sock, buffer, sizeof(buffer) - 1);

if (received > 0) {
    buffer[received] = '\0';
    printf("Received: %s\n", buffer);
}

net_socket_close(sock);
```

### 2. TCP Server

```c
// Create listening socket
net_socket_t *server = net_tcp_listen(8080, 10);
if (!server) {
    fprintf(stderr, "Failed to create server\n");
    return -1;
}

printf("Server listening on port 8080\n");

// Accept connections
while (1) {
    net_socket_t *client = net_tcp_accept(server);
    if (!client) {
        fprintf(stderr, "Accept failed\n");
        continue;
    }

    printf("Client connected\n");

    // Handle client
    char buffer[1024];
    int received = net_recv(client, buffer, sizeof(buffer));

    if (received > 0) {
        // Echo back
        net_send_all(client, buffer, received);
    }

    net_socket_close(client);
}

net_socket_close(server);
```

### 3. DNS Resolution

```c
uint32_t ip;
int rc = net_resolve("google.com", &ip);

if (rc == NET_ERR_NONE) {
    // Convert to readable format
    unsigned char *bytes = (unsigned char *)&ip;
    printf("IP: %d.%d.%d.%d\n", bytes[0], bytes[1], bytes[2], bytes[3]);
} else {
    fprintf(stderr, "DNS resolution failed\n");
}
```

### 4. Non-Blocking Socket

```c
net_socket_t *sock = net_tcp_connect("example.com", 80);

// Set non-blocking mode
net_set_blocking(sock, 0);

// Try to send (might return NET_ERR_WOULD_BLOCK)
int sent = net_send(sock, data, len);

if (sent == NET_ERR_WOULD_BLOCK) {
    // Socket would block, try again later
    // Use select() or poll() to wait for socket to be ready
}

net_socket_close(sock);
```

## API Reference

### Client Functions

#### `net_socket_t* net_tcp_connect(const char *host, int port)`
Connect to TCP server.
- **host**: Hostname or IP address
- **port**: TCP port number
- **Returns**: Socket object or NULL on error
- **Must close**: `net_socket_close(sock)`

### Server Functions

#### `net_socket_t* net_tcp_listen(int port, int backlog)`
Create TCP listening socket.
- **port**: Port to listen on
- **backlog**: Maximum pending connections
- **Returns**: Server socket or NULL on error

#### `net_socket_t* net_tcp_accept(net_socket_t *server)`
Accept incoming connection.
- **Returns**: Client socket or NULL on error
- **Blocks** until client connects

### Data Transfer

#### `int net_send(net_socket_t *sock, const void *data, size_t len)`
Send data (may send partial data).
- **Returns**: Bytes sent, or negative error code

#### `int net_send_all(net_socket_t *sock, const void *data, size_t len)`
Send all data (retries until complete).
- **Returns**: Total bytes sent, or negative on error

#### `int net_recv(net_socket_t *sock, void *buffer, size_t len)`
Receive data (returns when any data available).
- **Returns**: Bytes received, 0 on disconnect, negative on error

#### `int net_recv_all(net_socket_t *sock, void *buffer, size_t len)`
Receive exact amount of data (blocks until complete).
- **Returns**: Total bytes received, or negative on error

### Socket Control

#### `int net_set_blocking(net_socket_t *sock, int blocking)`
Set blocking/non-blocking mode.
- **blocking**: 1 for blocking, 0 for non-blocking
- **Returns**: 0 on success, negative on error

#### `int net_set_timeout(net_socket_t *sock, int timeout_ms)`
Set send/receive timeout.
- **timeout_ms**: Timeout in milliseconds
- **Returns**: 0 on success, negative on error

#### `void net_socket_close(net_socket_t *sock)`
Close socket and free resources.

### DNS Resolution

#### `int net_resolve(const char *hostname, uint32_t *ip)`
Resolve hostname to IP address.
- **ip**: Output IP in network byte order
- **Returns**: NET_ERR_NONE (0) on success

## Error Codes

```c
#define NET_ERR_NONE         0   /* No error */
#define NET_ERR_SOCKET      -1   /* Socket creation failed */
#define NET_ERR_CONNECT     -2   /* Connection failed */
#define NET_ERR_BIND        -3   /* Bind failed */
#define NET_ERR_LISTEN      -4   /* Listen failed */
#define NET_ERR_ACCEPT      -5   /* Accept failed */
#define NET_ERR_SEND        -6   /* Send failed */
#define NET_ERR_RECV        -7   /* Receive failed */
#define NET_ERR_TIMEOUT     -8   /* Operation timed out */
#define NET_ERR_WOULD_BLOCK -9   /* Would block (non-blocking mode) */
#define NET_ERR_RESOLVE    -10   /* DNS resolution failed */
#define NET_ERR_INVALID    -11   /* Invalid argument */
```

## Data Structures

```c
typedef enum {
    NET_STATE_DISCONNECTED,
    NET_STATE_CONNECTING,
    NET_STATE_CONNECTED,
    NET_STATE_LISTENING,
    NET_STATE_ERROR
} net_state_t;

typedef struct {
    int fd;                    // Socket file descriptor
    net_state_t state;         // Current state
    std_error_t *error;        // Last error (if any)
} net_socket_t;
```

## Error Handling

### Check Connection

```c
net_socket_t *sock = net_tcp_connect(host, port);
if (!sock) {
    fprintf(stderr, "Connection failed\n");
    return -1;
}

if (sock->state != NET_STATE_CONNECTED) {
    fprintf(stderr, "Socket not connected\n");
    net_socket_close(sock);
    return -1;
}
```

### Handle Send/Receive Errors

```c
int sent = net_send_all(sock, data, len);
if (sent < 0) {
    if (sock->error) {
        fprintf(stderr, "Send error: %s (code %d)\n",
                sock->error->message, sock->error->code);
    }
    net_socket_close(sock);
    return -1;
}
```

### Timeout Handling

```c
net_set_timeout(sock, 5000);  // 5 second timeout

int received = net_recv(sock, buffer, sizeof(buffer));
if (received < 0) {
    if (sock->error && sock->error->code == NET_ERR_TIMEOUT) {
        fprintf(stderr, "Receive timeout\n");
    }
}
```

## Performance Tips

1. **Buffer sizes**: Use appropriate buffer sizes (4KB-64KB typical)
   ```c
   char buffer[8192];  // 8KB buffer
   ```

2. **TCP_NODELAY**: Disable Nagle's algorithm for low-latency
   ```c
   int flag = 1;
   setsockopt(sock->fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));
   ```

3. **Socket buffers**: Increase for high throughput
   ```c
   int bufsize = 65536;  // 64KB
   setsockopt(sock->fd, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));
   setsockopt(sock->fd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));
   ```

4. **Reuse connections**: Keep connections alive for multiple requests

5. **Non-blocking I/O**: Use with select/poll for handling multiple connections

## Common Pitfalls

- **Partial sends**: Use `net_send_all()` instead of `net_send()` for complete data
- **Buffer overruns**: Always null-terminate received data if treating as string
- **Resource leaks**: Always call `net_socket_close()` when done
- **Byte order**: IP addresses returned in network byte order (use ntohl/htonl if needed)
- **Port numbers**: Valid range is 1-65535 (ports < 1024 require root on Unix)
- **Blocking**: Default mode is blocking (use `net_set_timeout()` to prevent hangs)

## Platform Differences

### Windows
- Uses Winsock2 (ws2_32.dll)
- Must call WSAStartup() before use (handled automatically)

### Unix/Linux
- Uses BSD sockets
- Signals (SIGPIPE) can interrupt operations

### macOS
- Same as Unix/Linux
- May require security permissions for server sockets

## Socket States

```
CLIENT:
  DISCONNECTED -> net_tcp_connect() -> CONNECTING -> CONNECTED -> net_socket_close() -> DISCONNECTED

SERVER:
  DISCONNECTED -> net_tcp_listen() -> LISTENING -> net_tcp_accept() -> CONNECTED (client socket)
```

## Example Output

```
========================================
  DNS Resolution Examples
========================================

1. Resolve Hostnames:
   google.com -> 142.250.185.46
   github.com -> 140.82.121.4
   localhost -> 127.0.0.1

========================================
  TCP Client Examples
========================================

1. Connect to Web Server:
   Connected to example.com:80

2. Send HTTP Request:
   Sent 47 bytes

3. Receive Response:
   Received 1256 bytes
   First 200 chars:
   HTTP/1.1 200 OK
   Date: Wed, 23 Oct 2024 10:30:00 GMT
   Content-Type: text/html; charset=UTF-8
   ...

✓ All network examples completed!
```

## Security Considerations

1. **Input validation**: Always validate hostnames and ports
2. **Buffer limits**: Prevent buffer overflows with proper size limits
3. **Timeouts**: Set timeouts to prevent DoS via slowloris attacks
4. **TLS/SSL**: Use `mod_curl` or external libraries for encrypted connections
5. **Error messages**: Don't leak sensitive information in error messages

## Related Modules

- **mod_http**: HTTP protocol built on top of mod_net
- **mod_curl**: Alternative HTTP with built-in SSL/TLS
- **mod_nng**: Nanomsg next-gen for advanced messaging patterns

## See Also

- Full example: `c_modules/examples/example_net.c`
- Socket programming: Beej's Guide to Network Programming
- TCP/IP: RFC 793 (TCP), RFC 791 (IP)
