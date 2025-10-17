/*
 * Futu OpenD API Client Test
 *
 * This program demonstrates connecting to Futu OpenD using protobuf protocol.
 * OpenD should be running locally on port 11111.
 *
 * Protocol:
 * - InitConnect: Initialize connection
 * - KeepAlive: Keep connection alive
 *
 * Build: cosmorun.exe futu_main.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "pb_encode.h"
#include "pb_decode.h"
#include "InitConnect.pb.h"
#include "KeepAlive.pb.h"
#include "Common.pb.h"

/* Futu protocol constants */
#define FUTU_PROTO_ID_INIT_CONNECT 1001
#define FUTU_PROTO_ID_KEEP_ALIVE 1004
#define FUTU_PROTO_FMT_PROTOBUF 0
#define FUTU_HEADER_SIZE 44

/* Futu protocol header structure */
typedef struct {
    uint8_t header_flag[2];     /* "FT" */
    uint8_t proto_id[4];        /* Protocol ID */
    uint8_t proto_fmt_type;     /* Format type (0=protobuf) */
    uint8_t proto_ver;          /* Protocol version */
    uint8_t serial_no[4];       /* Serial number */
    uint8_t body_len[4];        /* Body length */
    uint8_t body_sha1[20];      /* SHA1 hash of body */
    uint8_t reserved[8];        /* Reserved */
} futu_header_t;

/* Build Futu protocol header */
void build_futu_header(futu_header_t *header, uint32_t proto_id,
                       uint32_t serial_no, uint32_t body_len) {
    memset(header, 0, sizeof(futu_header_t));
    header->header_flag[0] = 'F';
    header->header_flag[1] = 'T';

    /* Protocol ID (big-endian) */
    header->proto_id[0] = (proto_id >> 24) & 0xFF;
    header->proto_id[1] = (proto_id >> 16) & 0xFF;
    header->proto_id[2] = (proto_id >> 8) & 0xFF;
    header->proto_id[3] = proto_id & 0xFF;

    header->proto_fmt_type = FUTU_PROTO_FMT_PROTOBUF;
    header->proto_ver = 0;

    /* Serial number (big-endian) */
    header->serial_no[0] = (serial_no >> 24) & 0xFF;
    header->serial_no[1] = (serial_no >> 16) & 0xFF;
    header->serial_no[2] = (serial_no >> 8) & 0xFF;
    header->serial_no[3] = serial_no & 0xFF;

    /* Body length (big-endian) */
    header->body_len[0] = (body_len >> 24) & 0xFF;
    header->body_len[1] = (body_len >> 16) & 0xFF;
    header->body_len[2] = (body_len >> 8) & 0xFF;
    header->body_len[3] = body_len & 0xFF;
}

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

/* Send InitConnect request */
int send_init_connect(int sock, uint32_t client_id) {
    InitConnect_C2S request = InitConnect_C2S_init_zero;
    request.clientVer = 1;
    request.clientID = client_id;
    strncpy(request.recvNotify, "false", sizeof(request.recvNotify) - 1);

    /* Encode protobuf message */
    uint8_t body_buffer[256];
    pb_ostream_t stream = pb_ostream_from_buffer(body_buffer, sizeof(body_buffer));

    if (!pb_encode(&stream, InitConnect_C2S_fields, &request)) {
        fprintf(stderr, "Encoding failed: %s\n", PB_GET_ERROR(&stream));
        return -1;
    }

    uint32_t body_len = stream.bytes_written;

    /* Build header */
    futu_header_t header;
    build_futu_header(&header, FUTU_PROTO_ID_INIT_CONNECT, 1, body_len);

    /* Send header + body */
    if (send(sock, &header, sizeof(header), 0) != sizeof(header)) {
        perror("send header");
        return -1;
    }

    if (send(sock, body_buffer, body_len, 0) != (ssize_t)body_len) {
        perror("send body");
        return -1;
    }

    printf("InitConnect sent (body_len=%u)\n", body_len);
    return 0;
}

/* Send KeepAlive request */
int send_keep_alive(int sock, uint32_t serial_no) {
    KeepAlive_C2S request = KeepAlive_C2S_init_zero;
    request.time = (int64_t)time(NULL);

    /* Encode protobuf message */
    uint8_t body_buffer[128];
    pb_ostream_t stream = pb_ostream_from_buffer(body_buffer, sizeof(body_buffer));

    if (!pb_encode(&stream, KeepAlive_C2S_fields, &request)) {
        fprintf(stderr, "Encoding failed: %s\n", PB_GET_ERROR(&stream));
        return -1;
    }

    uint32_t body_len = stream.bytes_written;

    /* Build header */
    futu_header_t header;
    build_futu_header(&header, FUTU_PROTO_ID_KEEP_ALIVE, serial_no, body_len);

    /* Send header + body */
    if (send(sock, &header, sizeof(header), 0) != sizeof(header)) {
        perror("send header");
        return -1;
    }

    if (send(sock, body_buffer, body_len, 0) != (ssize_t)body_len) {
        perror("send body");
        return -1;
    }

    printf("KeepAlive sent (time=%lld, serial=%u)\n",
           (long long)request.time, serial_no);
    return 0;
}

/* Receive and parse response */
int receive_response(int sock) {
    futu_header_t header;
    ssize_t n = recv(sock, &header, sizeof(header), 0);

    if (n <= 0) {
        if (n == 0) {
            printf("Connection closed by server\n");
        } else {
            perror("recv header");
        }
        return -1;
    }

    /* Parse header */
    uint32_t proto_id = ((uint32_t)header.proto_id[0] << 24) |
                        ((uint32_t)header.proto_id[1] << 16) |
                        ((uint32_t)header.proto_id[2] << 8) |
                        (uint32_t)header.proto_id[3];

    uint32_t body_len = ((uint32_t)header.body_len[0] << 24) |
                        ((uint32_t)header.body_len[1] << 16) |
                        ((uint32_t)header.body_len[2] << 8) |
                        (uint32_t)header.body_len[3];

    printf("Received response: proto_id=%u, body_len=%u\n", proto_id, body_len);

    /* Receive body */
    if (body_len > 0) {
        uint8_t *body = malloc(body_len);
        if (!body) {
            fprintf(stderr, "malloc failed\n");
            return -1;
        }

        n = recv(sock, body, body_len, 0);
        if (n != (ssize_t)body_len) {
            fprintf(stderr, "recv body failed\n");
            free(body);
            return -1;
        }

        printf("Body received (%u bytes)\n", body_len);
        free(body);
    }

    return 0;
}

int main(void) {
    printf("=== Futu OpenD Client Test ===\n\n");

    /* Connect to OpenD */
    const char *host = "127.0.0.1";
    int port = 11111;

    printf("Connecting to %s:%d...\n", host, port);
    int sock = connect_to_opend(host, port);
    if (sock < 0) {
        fprintf(stderr, "Failed to connect to OpenD\n");
        fprintf(stderr, "Make sure Futu OpenD is running on %s:%d\n", host, port);
        return 1;
    }

    printf("Connected successfully!\n\n");

    /* Send InitConnect */
    if (send_init_connect(sock, 100) < 0) {
        fprintf(stderr, "Failed to send InitConnect\n");
        close(sock);
        return 1;
    }

    /* Receive InitConnect response */
    if (receive_response(sock) < 0) {
        fprintf(stderr, "Failed to receive InitConnect response\n");
        close(sock);
        return 1;
    }

    printf("\n");

    /* Send KeepAlive */
    if (send_keep_alive(sock, 2) < 0) {
        fprintf(stderr, "Failed to send KeepAlive\n");
        close(sock);
        return 1;
    }

    /* Receive KeepAlive response */
    if (receive_response(sock) < 0) {
        fprintf(stderr, "Failed to receive KeepAlive response\n");
        close(sock);
        return 1;
    }

    printf("\n✓ Test completed successfully!\n");
    printf("Connection to Futu OpenD is working.\n");

    close(sock);
    return 0;
}
