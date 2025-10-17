/*
 * Futu OpenD Simple TCP Connection Test
 *
 * This program demonstrates basic TCP connection to Futu OpenD.
 * For full protobuf support, use GCC/Clang to compile.
 *
 * Build: ../cosmorun.exe futu_simple.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Connect to Futu OpenD */
int connect_to_opend(const char *host, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &server_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    return sock;
}

int main(void) {
    printf("=== Futu OpenD Simple Connection Test ===\n\n");

    const char *host = "127.0.0.1";
    int port = 11111;

    printf("Attempting to connect to %s:%d...\n", host, port);
    int sock = connect_to_opend(host, port);

    if (sock < 0) {
        printf("\n❌ Connection failed!\n");
        printf("Make sure Futu OpenD is running on %s:%d\n\n", host, port);
        printf("To install and start Futu OpenD:\n");
        printf("1. Download from: https://www.moomoo.com/download/OpenAPI\n");
        printf("2. Start OpenD and enable API access\n");
        printf("3. Configure port %d in OpenD settings\n\n", port);
        return 1;
    }

    printf("✓ Connected successfully!\n\n");
    printf("Connection Details:\n");
    printf("  Host: %s\n", host);
    printf("  Port: %d\n", port);
    printf("  Socket: %d\n\n", sock);

    printf("Next Steps:\n");
    printf("1. Use futu_main.c for full protobuf protocol support\n");
    printf("2. Compile with GCC: gcc futu_main.c pb_*.c *.pb.c -Ifutulab -o futu_client\n");
    printf("3. Download market data, place orders, etc.\n\n");

    printf("✓ Test completed successfully!\n");
    close(sock);
    return 0;
}
