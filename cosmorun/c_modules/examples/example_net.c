/*
 * example_net.c - Network Socket Examples
 *
 * Demonstrates:
 * - TCP client connections
 * - TCP server listening
 * - Sending and receiving data
 * - DNS resolution
 * - Error handling
 */

#include "src/cosmo_libc.h"
#include "c_modules/mod_net.c"

void example_tcp_client() {
    printf("\n========================================\n");
    printf("  TCP Client Examples\n");
    printf("========================================\n\n");

    /* Example 1: Simple TCP connection */
    printf("1. Connect to Web Server:\n");
    net_socket_t *sock = net_tcp_connect("example.com", 80);

    if (!sock) {
        fprintf(stderr, "   ERROR: Connection failed\n");
        return;
    }

    printf("   Connected to example.com:80\n");

    /* Example 2: Send HTTP request */
    printf("\n2. Send HTTP Request:\n");
    const char *request =
        "GET / HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Connection: close\r\n"
        "\r\n";

    int sent = net_send_all(sock, request, strlen(request));
    if (sent < 0) {
        fprintf(stderr, "   ERROR: Send failed\n");
        net_socket_close(sock);
        return;
    }

    printf("   Sent %d bytes\n", sent);

    /* Example 3: Receive response */
    printf("\n3. Receive Response:\n");
    char buffer[4096];
    int received = net_recv(sock, buffer, sizeof(buffer) - 1);

    if (received > 0) {
        buffer[received] = '\0';
        printf("   Received %d bytes\n", received);
        printf("   First 200 chars:\n");
        printf("   %.200s...\n", buffer);
    } else {
        fprintf(stderr, "   ERROR: Receive failed\n");
    }

    net_socket_close(sock);
    printf("\n");
}

void example_dns_resolution() {
    printf("\n========================================\n");
    printf("  DNS Resolution Examples\n");
    printf("========================================\n\n");

    printf("1. Resolve Hostnames:\n");
    const char *hosts[] = {
        "google.com",
        "github.com",
        "localhost"
    };

    for (int i = 0; i < 3; i++) {
        uint32_t ip;
        int rc = net_resolve(hosts[i], &ip);

        if (rc == NET_ERR_NONE) {
            /* Convert to dotted notation */
            unsigned char *bytes = (unsigned char *)&ip;
            printf("   %s -> %d.%d.%d.%d\n",
                   hosts[i], bytes[0], bytes[1], bytes[2], bytes[3]);
        } else {
            printf("   %s -> Resolution failed\n", hosts[i]);
        }
    }
    printf("\n");
}

void example_tcp_server() {
    printf("\n========================================\n");
    printf("  TCP Server Example\n");
    printf("========================================\n\n");

    printf("Creating TCP server on port 9000...\n");
    printf("(Server example - would block, showing concept only)\n\n");

    printf("Usage pattern:\n");
    printf("  // Create listening socket\n");
    printf("  net_socket_t *server = net_tcp_listen(9000, 5);\n");
    printf("  if (!server) {\n");
    printf("    fprintf(stderr, \"Failed to create server\\n\");\n");
    printf("    return -1;\n");
    printf("  }\n\n");

    printf("  // Accept client connections\n");
    printf("  while (1) {\n");
    printf("    net_socket_t *client = net_tcp_accept(server);\n");
    printf("    if (!client) continue;\n\n");

    printf("    // Handle client\n");
    printf("    char buffer[1024];\n");
    printf("    int received = net_recv(client, buffer, sizeof(buffer));\n");
    printf("    if (received > 0) {\n");
    printf("      net_send_all(client, \"Echo: \", 6);\n");
    printf("      net_send_all(client, buffer, received);\n");
    printf("    }\n\n");

    printf("    net_socket_close(client);\n");
    printf("  }\n\n");

    printf("  net_socket_close(server);\n");
    printf("\n");
}

void example_socket_options() {
    printf("\n========================================\n");
    printf("  Socket Options\n");
    printf("========================================\n\n");

    printf("1. Set Socket Options:\n");
    printf("   Usage:\n");
    printf("   net_socket_t *sock = net_tcp_connect(\"example.com\", 80);\n");
    printf("   \n");
    printf("   // Set TCP_NODELAY (disable Nagle's algorithm)\n");
    printf("   int flag = 1;\n");
    printf("   setsockopt(sock->fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));\n");
    printf("   \n");
    printf("   // Set send/receive buffer sizes\n");
    printf("   int bufsize = 32768;\n");
    printf("   setsockopt(sock->fd, SOL_SOCKET, SO_SNDBUF, &bufsize, sizeof(bufsize));\n");
    printf("   setsockopt(sock->fd, SOL_SOCKET, SO_RCVBUF, &bufsize, sizeof(bufsize));\n");
    printf("\n");
}

int main(void) {
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║  CosmoRun Network Module Examples     ║\n");
    printf("╚════════════════════════════════════════╝\n");

    example_dns_resolution();
    example_tcp_client();
    example_tcp_server();
    example_socket_options();

    printf("========================================\n");
    printf("  All network examples completed!\n");
    printf("========================================\n\n");

    return 0;
}
