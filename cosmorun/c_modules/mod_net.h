#ifndef MOD_NET_H
#define MOD_NET_H

/*
 * mod_net - Network utilities for cosmorun
 *
 * Provides cross-platform network functionality:
 * - TCP client/server
 * - UDP sockets
 * - DNS resolution
 * - Socket options
 * - Non-blocking I/O
 *
 * Platform support: Linux (primary), macOS, Windows (future)
 */

#include "../cosmorun_system/libc_shim/sys_libc_shim.h"
#include "mod_std.h"

/* ==================== Constants ==================== */

/* Socket types */
#define NET_SOCKET_TCP 1
#define NET_SOCKET_UDP 2

/* Socket states */
#define NET_STATE_CLOSED      0
#define NET_STATE_CONNECTING  1
#define NET_STATE_CONNECTED   2
#define NET_STATE_LISTENING   3
#define NET_STATE_ERROR       4

/* Error codes */
#define NET_ERR_NONE          0
#define NET_ERR_SOCKET       -1
#define NET_ERR_CONNECT      -2
#define NET_ERR_BIND         -3
#define NET_ERR_LISTEN       -4
#define NET_ERR_ACCEPT       -5
#define NET_ERR_SEND         -6
#define NET_ERR_RECV         -7
#define NET_ERR_RESOLVE      -8
#define NET_ERR_TIMEOUT      -9
#define NET_ERR_CLOSED      -10
#define NET_ERR_INVALID     -11

/* Timeout values (milliseconds) */
#define NET_TIMEOUT_INFINITE -1
#define NET_TIMEOUT_DEFAULT  30000  /* 30 seconds */

/* ==================== Data Structures ==================== */

/* Socket address structure */
typedef struct {
    uint32_t ip;        /* IPv4 address in network byte order */
    uint16_t port;      /* Port in host byte order */
    char *hostname;     /* Original hostname (optional) */
} net_addr_t;

/* Socket structure */
typedef struct {
    int fd;             /* File descriptor */
    int type;           /* NET_SOCKET_TCP or NET_SOCKET_UDP */
    int state;          /* Current state */
    net_addr_t local;   /* Local address */
    net_addr_t remote;  /* Remote address */
    int timeout_ms;     /* Read/write timeout in milliseconds */
    std_error_t *error; /* Last error */
} net_socket_t;

/* ==================== DNS Resolution ==================== */

/*
 * Resolve hostname to IPv4 address
 * Returns: 0 on success, NET_ERR_RESOLVE on failure
 */
int net_resolve(const char *hostname, uint32_t *ip);

/*
 * Convert IPv4 address to string (e.g., "192.168.1.1")
 * Returns: string representation (must be freed by caller)
 */
char* net_ip_to_string(uint32_t ip);

/*
 * Parse IPv4 string to address (e.g., "192.168.1.1")
 * Returns: 0 on success, NET_ERR_INVALID on failure
 */
int net_string_to_ip(const char *str, uint32_t *ip);

/* ==================== TCP Client ==================== */

/*
 * Create and connect TCP socket to remote host
 * Args:
 *   host - hostname or IPv4 address string
 *   port - port number (1-65535)
 * Returns: socket on success, NULL on failure
 */
net_socket_t* net_tcp_connect(const char *host, int port);

/*
 * Connect TCP socket with timeout
 * Returns: socket on success, NULL on failure
 */
net_socket_t* net_tcp_connect_timeout(const char *host, int port, int timeout_ms);

/* ==================== TCP Server ==================== */

/*
 * Create TCP listening socket
 * Args:
 *   port - port to listen on (0 = any available port)
 *   backlog - connection queue size (default 5)
 * Returns: socket on success, NULL on failure
 */
net_socket_t* net_tcp_listen(int port, int backlog);

/*
 * Accept incoming TCP connection
 * Args:
 *   server - listening socket
 * Returns: client socket on success, NULL on failure
 */
net_socket_t* net_tcp_accept(net_socket_t *server);

/*
 * Accept with timeout
 * Returns: client socket on success, NULL on timeout/failure
 */
net_socket_t* net_tcp_accept_timeout(net_socket_t *server, int timeout_ms);

/* ==================== UDP Sockets ==================== */

/*
 * Create UDP socket
 * Args:
 *   port - local port to bind (0 = any available port)
 * Returns: socket on success, NULL on failure
 */
net_socket_t* net_udp_socket(int port);

/*
 * Send UDP packet
 * Returns: bytes sent on success, negative on failure
 */
int net_udp_send(net_socket_t *sock, const char *host, int port,
                 const void *data, size_t len);

/*
 * Receive UDP packet
 * Args:
 *   sock - UDP socket
 *   buf - buffer to receive data
 *   len - buffer size
 *   sender - filled with sender's address (can be NULL)
 * Returns: bytes received on success, negative on failure
 */
int net_udp_recv(net_socket_t *sock, void *buf, size_t len, net_addr_t *sender);

/* ==================== Socket I/O ==================== */

/*
 * Send data through TCP socket
 * Returns: bytes sent on success, negative on failure
 */
int net_send(net_socket_t *sock, const void *data, size_t len);

/*
 * Receive data from TCP socket
 * Returns: bytes received on success, 0 on EOF, negative on failure
 */
int net_recv(net_socket_t *sock, void *buf, size_t len);

/*
 * Send all data (blocks until all sent or error)
 * Returns: 0 on success, negative on failure
 */
int net_send_all(net_socket_t *sock, const void *data, size_t len);

/*
 * Receive exact amount of data (blocks until all received or error)
 * Returns: 0 on success, negative on failure
 */
int net_recv_all(net_socket_t *sock, void *buf, size_t len);

/* ==================== Socket Options ==================== */

/*
 * Set socket timeout for read/write operations
 * Args:
 *   timeout_ms - timeout in milliseconds (NET_TIMEOUT_INFINITE for no timeout)
 * Returns: 0 on success, negative on failure
 */
int net_set_timeout(net_socket_t *sock, int timeout_ms);

/*
 * Enable/disable non-blocking mode
 * Returns: 0 on success, negative on failure
 */
int net_set_nonblocking(net_socket_t *sock, int enable);

/*
 * Enable/disable TCP_NODELAY (disable Nagle's algorithm)
 * Returns: 0 on success, negative on failure
 */
int net_set_nodelay(net_socket_t *sock, int enable);

/*
 * Enable/disable SO_REUSEADDR
 * Returns: 0 on success, negative on failure
 */
int net_set_reuseaddr(net_socket_t *sock, int enable);

/*
 * Set send buffer size
 * Returns: 0 on success, negative on failure
 */
int net_set_sendbuf(net_socket_t *sock, int size);

/*
 * Set receive buffer size
 * Returns: 0 on success, negative on failure
 */
int net_set_recvbuf(net_socket_t *sock, int size);

/* ==================== Socket Management ==================== */

/*
 * Close socket and free resources
 */
void net_socket_close(net_socket_t *sock);

/*
 * Get last error from socket
 * Returns: error structure (do not free)
 */
std_error_t* net_socket_error(net_socket_t *sock);

/*
 * Get local address and port
 * Returns: 0 on success, negative on failure
 */
int net_socket_local_addr(net_socket_t *sock, net_addr_t *addr);

/*
 * Get remote address and port
 * Returns: 0 on success, negative on failure
 */
int net_socket_remote_addr(net_socket_t *sock, net_addr_t *addr);

/* ==================== Utility Functions ==================== */

/*
 * Convert port from host to network byte order
 */
uint16_t net_htons(uint16_t port);

/*
 * Convert port from network to host byte order
 */
uint16_t net_ntohs(uint16_t port);

/*
 * Convert IPv4 address from host to network byte order
 */
uint32_t net_htonl(uint32_t ip);

/*
 * Convert IPv4 address from network to host byte order
 */
uint32_t net_ntohl(uint32_t ip);

#endif /* MOD_NET_H */
