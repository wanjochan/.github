/*
 * mod_net - Network utilities implementation
 */

#include "mod_net.h"

#ifndef __COSMORUN__
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#else
/* Additional network definitions for __COSMORUN__ mode */
#define INET_ADDRSTRLEN 16
#define SOCK_DGRAM 2
#define TCP_NODELAY 1
#define SO_SNDBUF 7
#define SO_RCVBUF 8

/* hostent structure for DNS resolution */
struct hostent {
    char *h_name;
    char **h_aliases;
    int h_addrtype;
    int h_length;
    char **h_addr_list;
};

/* Network function declarations */
extern struct hostent *gethostbyname(const char *name);
extern const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);
extern int inet_pton(int af, const char *src, void *dst);

/* Socket system calls */
extern int socket(int domain, int type, int protocol);
extern int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
extern int listen(int sockfd, int backlog);
extern int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
extern int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
extern int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
extern int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
extern int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
extern int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);

/* Socket I/O */
extern ssize_t send(int sockfd, const void *buf, size_t len, int flags);
extern ssize_t recv(int sockfd, void *buf, size_t len, int flags);
extern ssize_t sendto(int sockfd, const void *buf, size_t len, int flags,
                      const struct sockaddr *dest_addr, socklen_t addrlen);
extern ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags,
                        struct sockaddr *src_addr, socklen_t *addrlen);

/* Byte order conversion */
extern uint16_t htons(uint16_t hostshort);
extern uint16_t ntohs(uint16_t netshort);
extern uint32_t htonl(uint32_t hostlong);
extern uint32_t ntohl(uint32_t netlong);
#endif

/* ==================== Helper Functions ==================== */

static void net_set_error(net_socket_t *sock, int code, const char *message) {
    if (sock->error) {
        std_error_free(sock->error);
    }
    sock->error = std_error_new(code, message);
    sock->state = NET_STATE_ERROR;
}

static void net_clear_error(net_socket_t *sock) {
    if (sock->error) {
        std_error_free(sock->error);
        sock->error = NULL;
    }
}

/* ==================== DNS Resolution ==================== */

int net_resolve(const char *hostname, uint32_t *ip) {
    if (!hostname || !ip) {
        return NET_ERR_INVALID;
    }

    /* Try parsing as IPv4 address first */
    struct in_addr addr;
    if (inet_pton(AF_INET, hostname, &addr) == 1) {
        *ip = addr.s_addr;
        return NET_ERR_NONE;
    }

    /* DNS lookup */
    struct hostent *he = gethostbyname(hostname);
    if (!he || !he->h_addr_list[0]) {
        return NET_ERR_RESOLVE;
    }

    *ip = *(uint32_t*)he->h_addr_list[0];
    return NET_ERR_NONE;
}

char* net_ip_to_string(uint32_t ip) {
    struct in_addr addr;
    addr.s_addr = ip;

    char buf[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET, &addr, buf, sizeof(buf))) {
        return NULL;
    }

    return strdup(buf);
}

int net_string_to_ip(const char *str, uint32_t *ip) {
    if (!str || !ip) {
        return NET_ERR_INVALID;
    }

    struct in_addr addr;
    if (inet_pton(AF_INET, str, &addr) != 1) {
        return NET_ERR_INVALID;
    }

    *ip = addr.s_addr;
    return NET_ERR_NONE;
}

/* ==================== Socket Creation ==================== */

static net_socket_t* net_socket_new(int type) {
    net_socket_t *sock = (net_socket_t*)malloc(sizeof(net_socket_t));
    if (!sock) {
        return NULL;
    }

    sock->fd = -1;
    sock->type = type;
    sock->state = NET_STATE_CLOSED;
    sock->local.ip = 0;
    sock->local.port = 0;
    sock->local.hostname = NULL;
    sock->remote.ip = 0;
    sock->remote.port = 0;
    sock->remote.hostname = NULL;
    sock->timeout_ms = NET_TIMEOUT_DEFAULT;
    sock->error = NULL;

    return sock;
}

/* ==================== TCP Client ==================== */

net_socket_t* net_tcp_connect(const char *host, int port) {
    return net_tcp_connect_timeout(host, port, NET_TIMEOUT_DEFAULT);
}

net_socket_t* net_tcp_connect_timeout(const char *host, int port, int timeout_ms) {
    if (!host || port <= 0 || port > 65535) {
        return NULL;
    }

    net_socket_t *sock = net_socket_new(NET_SOCKET_TCP);
    if (!sock) {
        return NULL;
    }

    /* Resolve hostname */
    uint32_t ip;
    if (net_resolve(host, &ip) != NET_ERR_NONE) {
        net_set_error(sock, NET_ERR_RESOLVE, "Failed to resolve hostname");
        return sock;
    }

    /* Create socket */
    sock->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock->fd < 0) {
        net_set_error(sock, NET_ERR_SOCKET, "Failed to create socket");
        return sock;
    }

    /* Set timeout */
    if (timeout_ms != NET_TIMEOUT_INFINITE) {
        sock->timeout_ms = timeout_ms;
        net_set_timeout(sock, timeout_ms);
    }

    /* Connect */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ip;
    addr.sin_port = htons(port);

    sock->state = NET_STATE_CONNECTING;
    if (connect(sock->fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        net_set_error(sock, NET_ERR_CONNECT, "Connection failed");
        close(sock->fd);
        sock->fd = -1;
        return sock;
    }

    /* Success */
    sock->state = NET_STATE_CONNECTED;
    sock->remote.ip = ip;
    sock->remote.port = port;
    sock->remote.hostname = strdup(host);
    net_clear_error(sock);

    return sock;
}

/* ==================== TCP Server ==================== */

net_socket_t* net_tcp_listen(int port, int backlog) {
    if (port < 0 || port > 65535) {
        return NULL;
    }

    if (backlog <= 0) {
        backlog = 5;
    }

    net_socket_t *sock = net_socket_new(NET_SOCKET_TCP);
    if (!sock) {
        return NULL;
    }

    /* Create socket */
    sock->fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock->fd < 0) {
        net_set_error(sock, NET_ERR_SOCKET, "Failed to create socket");
        return sock;
    }

    /* Enable address reuse */
    int reuse = 1;
    setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    /* Bind */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sock->fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        net_set_error(sock, NET_ERR_BIND, "Failed to bind socket");
        close(sock->fd);
        sock->fd = -1;
        return sock;
    }

    /* Get actual port if 0 was specified */
    if (port == 0) {
        socklen_t len = sizeof(addr);
        if (getsockname(sock->fd, (struct sockaddr*)&addr, &len) == 0) {
            port = ntohs(addr.sin_port);
        }
    }

    /* Listen */
    if (listen(sock->fd, backlog) < 0) {
        net_set_error(sock, NET_ERR_LISTEN, "Failed to listen on socket");
        close(sock->fd);
        sock->fd = -1;
        return sock;
    }

    /* Success */
    sock->state = NET_STATE_LISTENING;
    sock->local.port = port;
    net_clear_error(sock);

    return sock;
}

net_socket_t* net_tcp_accept(net_socket_t *server) {
    return net_tcp_accept_timeout(server, NET_TIMEOUT_INFINITE);
}

net_socket_t* net_tcp_accept_timeout(net_socket_t *server, int timeout_ms) {
    if (!server || server->state != NET_STATE_LISTENING) {
        return NULL;
    }

    /* Set timeout if specified */
    if (timeout_ms != NET_TIMEOUT_INFINITE) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server->fd, &readfds);

        struct timeval tv;
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        int ret = select(server->fd + 1, &readfds, NULL, NULL, &tv);
        if (ret <= 0) {
            return NULL;  /* Timeout or error */
        }
    }

    /* Accept connection */
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    int client_fd = accept(server->fd, (struct sockaddr*)&addr, &addr_len);

    if (client_fd < 0) {
        return NULL;
    }

    /* Create client socket */
    net_socket_t *client = net_socket_new(NET_SOCKET_TCP);
    if (!client) {
        close(client_fd);
        return NULL;
    }

    client->fd = client_fd;
    client->state = NET_STATE_CONNECTED;
    client->remote.ip = addr.sin_addr.s_addr;
    client->remote.port = ntohs(addr.sin_port);
    client->timeout_ms = server->timeout_ms;

    return client;
}

/* ==================== UDP Sockets ==================== */

net_socket_t* net_udp_socket(int port) {
    if (port < 0 || port > 65535) {
        return NULL;
    }

    net_socket_t *sock = net_socket_new(NET_SOCKET_UDP);
    if (!sock) {
        return NULL;
    }

    /* Create socket */
    sock->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock->fd < 0) {
        net_set_error(sock, NET_ERR_SOCKET, "Failed to create UDP socket");
        return sock;
    }

    /* Bind if port specified */
    if (port > 0) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);

        if (bind(sock->fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            net_set_error(sock, NET_ERR_BIND, "Failed to bind UDP socket");
            close(sock->fd);
            sock->fd = -1;
            return sock;
        }

        sock->local.port = port;
    }

    sock->state = NET_STATE_CONNECTED;
    net_clear_error(sock);

    return sock;
}

int net_udp_send(net_socket_t *sock, const char *host, int port,
                 const void *data, size_t len) {
    if (!sock || !host || !data || sock->type != NET_SOCKET_UDP) {
        return NET_ERR_INVALID;
    }

    /* Resolve hostname */
    uint32_t ip;
    if (net_resolve(host, &ip) != NET_ERR_NONE) {
        return NET_ERR_RESOLVE;
    }

    /* Send packet */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ip;
    addr.sin_port = htons(port);

    int ret = sendto(sock->fd, data, len, 0,
                     (struct sockaddr*)&addr, sizeof(addr));

    if (ret < 0) {
        net_set_error(sock, NET_ERR_SEND, "UDP send failed");
        return NET_ERR_SEND;
    }

    return ret;
}

int net_udp_recv(net_socket_t *sock, void *buf, size_t len, net_addr_t *sender) {
    if (!sock || !buf || sock->type != NET_SOCKET_UDP) {
        return NET_ERR_INVALID;
    }

    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    int ret = recvfrom(sock->fd, buf, len, 0,
                       (struct sockaddr*)&addr, &addr_len);

    if (ret < 0) {
        net_set_error(sock, NET_ERR_RECV, "UDP receive failed");
        return NET_ERR_RECV;
    }

    /* Fill sender info if requested */
    if (sender) {
        sender->ip = addr.sin_addr.s_addr;
        sender->port = ntohs(addr.sin_port);
        sender->hostname = NULL;
    }

    return ret;
}

/* ==================== Socket I/O ==================== */

int net_send(net_socket_t *sock, const void *data, size_t len) {
    if (!sock || !data || sock->fd < 0) {
        return NET_ERR_INVALID;
    }

    int ret = send(sock->fd, data, len, 0);
    if (ret < 0) {
        net_set_error(sock, NET_ERR_SEND, "Send failed");
        return NET_ERR_SEND;
    }

    return ret;
}

int net_recv(net_socket_t *sock, void *buf, size_t len) {
    if (!sock || !buf || sock->fd < 0) {
        return NET_ERR_INVALID;
    }

    int ret = recv(sock->fd, buf, len, 0);
    if (ret < 0) {
        net_set_error(sock, NET_ERR_RECV, "Receive failed");
        return NET_ERR_RECV;
    }

    return ret;
}

int net_send_all(net_socket_t *sock, const void *data, size_t len) {
    if (!sock || !data || sock->fd < 0) {
        return NET_ERR_INVALID;
    }

    const char *ptr = (const char*)data;
    size_t remaining = len;

    while (remaining > 0) {
        int sent = send(sock->fd, ptr, remaining, 0);
        if (sent < 0) {
            net_set_error(sock, NET_ERR_SEND, "Send failed");
            return NET_ERR_SEND;
        }

        ptr += sent;
        remaining -= sent;
    }

    return NET_ERR_NONE;
}

int net_recv_all(net_socket_t *sock, void *buf, size_t len) {
    if (!sock || !buf || sock->fd < 0) {
        return NET_ERR_INVALID;
    }

    char *ptr = (char*)buf;
    size_t remaining = len;

    while (remaining > 0) {
        int received = recv(sock->fd, ptr, remaining, 0);
        if (received < 0) {
            net_set_error(sock, NET_ERR_RECV, "Receive failed");
            return NET_ERR_RECV;
        }

        if (received == 0) {
            /* Connection closed */
            net_set_error(sock, NET_ERR_CLOSED, "Connection closed");
            return NET_ERR_CLOSED;
        }

        ptr += received;
        remaining -= received;
    }

    return NET_ERR_NONE;
}

/* ==================== Socket Options ==================== */

int net_set_timeout(net_socket_t *sock, int timeout_ms) {
    if (!sock || sock->fd < 0) {
        return NET_ERR_INVALID;
    }

    struct timeval tv;
    if (timeout_ms == NET_TIMEOUT_INFINITE) {
        tv.tv_sec = 0;
        tv.tv_usec = 0;
    } else {
        tv.tv_sec = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;
    }

    if (setsockopt(sock->fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        return NET_ERR_INVALID;
    }

    if (setsockopt(sock->fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        return NET_ERR_INVALID;
    }

    sock->timeout_ms = timeout_ms;
    return NET_ERR_NONE;
}

int net_set_nonblocking(net_socket_t *sock, int enable) {
    if (!sock || sock->fd < 0) {
        return NET_ERR_INVALID;
    }

    int flags = fcntl(sock->fd, F_GETFL, 0);
    if (flags < 0) {
        return NET_ERR_INVALID;
    }

    if (enable) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }

    if (fcntl(sock->fd, F_SETFL, flags) < 0) {
        return NET_ERR_INVALID;
    }

    return NET_ERR_NONE;
}

int net_set_nodelay(net_socket_t *sock, int enable) {
    if (!sock || sock->fd < 0 || sock->type != NET_SOCKET_TCP) {
        return NET_ERR_INVALID;
    }

#ifdef TCP_NODELAY
    int flag = enable ? 1 : 0;
    if (setsockopt(sock->fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag)) < 0) {
        return NET_ERR_INVALID;
    }
#endif

    return NET_ERR_NONE;
}

int net_set_reuseaddr(net_socket_t *sock, int enable) {
    if (!sock || sock->fd < 0) {
        return NET_ERR_INVALID;
    }

    int flag = enable ? 1 : 0;
    if (setsockopt(sock->fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) < 0) {
        return NET_ERR_INVALID;
    }

    return NET_ERR_NONE;
}

int net_set_sendbuf(net_socket_t *sock, int size) {
    if (!sock || sock->fd < 0 || size <= 0) {
        return NET_ERR_INVALID;
    }

    if (setsockopt(sock->fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size)) < 0) {
        return NET_ERR_INVALID;
    }

    return NET_ERR_NONE;
}

int net_set_recvbuf(net_socket_t *sock, int size) {
    if (!sock || sock->fd < 0 || size <= 0) {
        return NET_ERR_INVALID;
    }

    if (setsockopt(sock->fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size)) < 0) {
        return NET_ERR_INVALID;
    }

    return NET_ERR_NONE;
}

/* ==================== Socket Management ==================== */

void net_socket_close(net_socket_t *sock) {
    if (!sock) {
        return;
    }

    if (sock->fd >= 0) {
        close(sock->fd);
        sock->fd = -1;
    }

    if (sock->local.hostname) {
        free(sock->local.hostname);
    }

    if (sock->remote.hostname) {
        free(sock->remote.hostname);
    }

    if (sock->error) {
        std_error_free(sock->error);
    }

    sock->state = NET_STATE_CLOSED;
    free(sock);
}

std_error_t* net_socket_error(net_socket_t *sock) {
    if (!sock) {
        return NULL;
    }
    return sock->error;
}

int net_socket_local_addr(net_socket_t *sock, net_addr_t *addr) {
    if (!sock || !addr || sock->fd < 0) {
        return NET_ERR_INVALID;
    }

    struct sockaddr_in sa;
    socklen_t len = sizeof(sa);

    if (getsockname(sock->fd, (struct sockaddr*)&sa, &len) < 0) {
        return NET_ERR_INVALID;
    }

    addr->ip = sa.sin_addr.s_addr;
    addr->port = ntohs(sa.sin_port);
    addr->hostname = NULL;

    return NET_ERR_NONE;
}

int net_socket_remote_addr(net_socket_t *sock, net_addr_t *addr) {
    if (!sock || !addr || sock->fd < 0) {
        return NET_ERR_INVALID;
    }

    struct sockaddr_in sa;
    socklen_t len = sizeof(sa);

    if (getpeername(sock->fd, (struct sockaddr*)&sa, &len) < 0) {
        return NET_ERR_INVALID;
    }

    addr->ip = sa.sin_addr.s_addr;
    addr->port = ntohs(sa.sin_port);
    addr->hostname = NULL;

    return NET_ERR_NONE;
}

/* ==================== Utility Functions ==================== */

uint16_t net_htons(uint16_t port) {
    return htons(port);
}

uint16_t net_ntohs(uint16_t port) {
    return ntohs(port);
}

uint32_t net_htonl(uint32_t ip) {
    return htonl(ip);
}

uint32_t net_ntohl(uint32_t ip) {
    return ntohl(ip);
}
