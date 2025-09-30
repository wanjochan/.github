/**
 * CDP WebSocket Module
 * Handles WebSocket connections and communication
 */

#include "cdp_internal.h"
/* #include "cdp_log.h" - merged into cdp_internal.h */
/* #include "cdp_bus.h" - merged into cdp_internal.h */
#include <fcntl.h>
#include <errno.h>
#include <sys/select.h>

/* External global variables */
extern CDPContext g_ctx;
extern int verbose;
extern int ws_sock;
extern int ws_cmd_id;

/* Static variables for this module */
static WebSocketClient g_ws_client = {.socket = -1, .connected = 0};
static RetryConfig reconnect_config = {
    .max_attempts = 5,
    .base_delay_ms = 100,
    .backoff_factor = 2.0,
    .max_delay_ms = 5000
};

/* Forward declarations for internal functions */
static int ws_build_text_frame(uint8_t *frame, size_t *frame_size, const char *text, size_t len);
static int ws_receive_and_decode_frame(int sock, char *buffer, size_t max_len);

/* Send text through WebSocket (hides frame encoding) */
int ws_send_text(int sock, const char *text) {
    if (sock < 0 || !text) return -1;
    
    DEBUG_LOG("WS Send: %.200s%s", text, strlen(text) > 200 ? "..." : "");
    
    size_t text_len = strlen(text);
    size_t frame_size = 10 + text_len;  // Max header + payload
    uint8_t *frame = (uint8_t*)safe_malloc(frame_size);
    if (!frame) return -1;
    
    // Build WebSocket frame (encapsulated)
    size_t actual_size = 0;
    if (ws_build_text_frame(frame, &actual_size, text, text_len) != 0) {
        safe_free((void**)&frame);
        return -1;
    }
    
    int result = send(sock, frame, actual_size, 0);
    safe_free((void**)&frame);
    return result;
}

/* Build WebSocket text frame (internal detail) */
static int ws_build_text_frame(uint8_t *frame, size_t *frame_size, const char *text, size_t len) {
    int pos = 0;
    
    // FIN=1, Opcode=1 (text) - protocol detail hidden here
    frame[pos++] = 0x81;
    
    // Payload length with mask bit
    if (len < 126) {
        frame[pos++] = 0x80 | len;
    } else if (len < 65536) {
        frame[pos++] = 0x80 | 126;
        frame[pos++] = (len >> 8) & 0xFF;
        frame[pos++] = len & 0xFF;
    } else {
        return -1;  // Too large
    }
    
    // Masking key (client must mask)
    uint8_t mask[4];
    for (int i = 0; i < 4; i++) {
        mask[i] = rand() & 0xFF;
        frame[pos++] = mask[i];
    }
    
    // Masked payload
    for (size_t i = 0; i < len; i++) {
        frame[pos++] = text[i] ^ mask[i % 4];
    }
    
    *frame_size = pos;
    return 0;
}

/* Receive text from WebSocket (hides frame decoding) */
int ws_recv_text(int sock, char *buffer, size_t max_len) {
    if (sock < 0 || !buffer || max_len == 0) return -1;
    
    int result = ws_receive_and_decode_frame(sock, buffer, max_len);
    
    if (result > 0) {
        DEBUG_LOG("WS Recv: %.200s%s", buffer, strlen(buffer) > 200 ? "..." : "");
    }
    
    return result;
}

/* Receive and decode WebSocket frame (internal) */
static int ws_receive_and_decode_frame(int sock, char *buffer, size_t max_len) {
    uint8_t header[2];
    int ret;
    
    // Read frame header
    ret = recv(sock, header, 2, 0);
    if (ret != 2) {
        if (ret == 0) {
            DEBUG_LOG("WebSocket closed by peer");
        } else if (ret < 0) {
            DEBUG_LOG("WebSocket recv error: %s", strerror(errno));
        }
        return -1;
    }
    
    uint8_t fin = (header[0] >> 7) & 1;
    uint8_t opcode = header[0] & 0x0F;
    uint8_t mask = (header[1] >> 7) & 1;
    uint64_t payload_len = header[1] & 0x7F;
    
    // Extended payload length
    if (payload_len == 126) {
        uint8_t ext[2];
        if (recv(sock, ext, 2, 0) != 2) return -1;
        payload_len = (ext[0] << 8) | ext[1];
    } else if (payload_len == 127) {
        uint8_t ext[8];
        if (recv(sock, ext, 8, 0) != 8) return -1;
        payload_len = 0;
        for (int i = 0; i < 8; i++) {
            payload_len = (payload_len << 8) | ext[i];
        }
    }
    
    // Check if payload fits in buffer
    if (payload_len >= max_len) {
        DEBUG_LOG("WebSocket payload too large: %llu bytes", (unsigned long long)payload_len);
        return -1;
    }
    
    // Masking key (server shouldn't mask)
    uint8_t mask_key[4] = {0};
    if (mask) {
        if (recv(sock, mask_key, 4, 0) != 4) return -1;
    }
    
    // Read payload
    size_t total_read = 0;
    while (total_read < payload_len) {
        ssize_t bytes = recv(sock, buffer + total_read, payload_len - total_read, 0);
        if (bytes <= 0) {
            DEBUG_LOG("WebSocket recv payload error");
            return -1;
        }
        total_read += bytes;
    }
    
    // Unmask if needed
    if (mask) {
        for (uint64_t i = 0; i < payload_len; i++) {
            buffer[i] ^= mask_key[i % 4];
        }
    }
    
    buffer[payload_len] = '\0';
    
    // Handle different opcodes
    switch (opcode) {
        case 0x1:  // Text frame
            return payload_len;
        case 0x8:  // Close frame
            DEBUG_LOG("WebSocket close frame received");
            return -1;
        case 0x9:  // Ping frame
            DEBUG_LOG("WebSocket ping received");
            // Send pong (we should implement this)
            return 0;
        case 0xA:  // Pong frame
            DEBUG_LOG("WebSocket pong received");
            return 0;
        default:
            DEBUG_LOG("WebSocket unknown opcode: 0x%x", opcode);
            return 0;
    }
}

/* Connect to Chrome via WebSocket */
int connect_chrome_websocket(const char *target_id) {
    if (!target_id) { cdp_log(CDP_LOG_ERR, "WS", "No target ID provided"); return -1; }
    
    // Create TCP socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { cdp_log(CDP_LOG_ERR, "WS", "socket() failed: %s", strerror(errno)); return -1; }
    
    // Connect to Chrome
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(g_ctx.config.debug_port);
    addr.sin_addr.s_addr = inet_addr(g_ctx.config.chrome_host);
    
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) { cdp_log(CDP_LOG_ERR, "WS", "connect() failed: %s", strerror(errno)); close(sock); return -1; }
    
    // Build WebSocket handshake
    char key[25];
    unsigned char raw_key[16];
    for (int i = 0; i < 16; i++) raw_key[i] = rand() & 0xFF;
    ws_base64_encode(raw_key, 16, key, sizeof(key));
    
    char request[1024];
    snprintf(request, sizeof(request),
        "GET /devtools/%s HTTP/1.1\r\n"
        "Host: %s:%d\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Key: %s\r\n"
        "Sec-WebSocket-Version: 13\r\n"
        "\r\n",
        target_id, g_ctx.config.chrome_host, g_ctx.config.debug_port, key);
    
    // Send handshake
    if (send(sock, request, strlen(request), 0) < 0) {
        cdp_log(CDP_LOG_ERR, "WS", "Handshake send failed: %s", strerror(errno));
        close(sock);
        return -1;
    }
    
    // Read response
    char response[1024];
    int len = recv(sock, response, sizeof(response)-1, 0);
    if (len <= 0) {
        close(sock);
        return -1;
    }
    response[len] = '\0';
    
    // Check for successful upgrade
    if (!strstr(response, "101")) {
        cdp_log(CDP_LOG_ERR, "WS", "Handshake failed: %.120s", response);
        close(sock);
        return -1;
    }
    
    // Enable Runtime if this is a page endpoint
    if (strstr(target_id, "page/")) {
        if (cdp_send_cmd("Runtime.enable", NULL) >= 0) {
            g_ctx.runtime.runtime_ready = 1;
        }
    }
    
    g_ws_client.socket = sock;
    g_ws_client.connected = 1;
    return sock;
}

/* Reconnect WebSocket with exponential backoff */
int reconnect_websocket_with_backoff(void) {
    // Show reconnection status to user
    if (!verbose && g_ctx.conn.reconnect_attempts == 0) {
        fprintf(stderr, "\nConnection lost. Reconnecting");
        fflush(stderr);
    }
    
    DEBUG_LOG("WebSocket reconnection attempt %d/%d", 
              g_ctx.conn.reconnect_attempts + 1, 
              g_ctx.conn.max_reconnect_attempts);
    
    if (g_ctx.conn.reconnect_attempts >= g_ctx.conn.max_reconnect_attempts) {
        cdp_log(CDP_LOG_ERR, "WS", "Max reconnection attempts reached. Giving up.");
        cdp_log(CDP_LOG_ERR, "WS", "Check if Chrome is running on port %d", g_ctx.config.debug_port);
        return -1;
    }
    
    // Close existing socket if open
    if (ws_sock >= 0) {
        close(ws_sock);
        ws_sock = -1;
    }
    
    // Exponential backoff delay
    if (g_ctx.conn.reconnect_attempts > 0) {
        int delay_ms = g_ctx.conn.reconnect_delay_ms * (1 << (g_ctx.conn.reconnect_attempts - 1));
        if (delay_ms > 30000) delay_ms = 30000;  // Cap at 30 seconds
        
        if (!verbose) { fprintf(stderr, "."); fflush(stderr); }
        DEBUG_LOG("Waiting %dms before reconnection attempt", delay_ms);
        usleep(delay_ms * 1000);
    }
    
    g_ctx.conn.reconnect_attempts++;
    
    // Try to reconnect
    ws_sock = connect_chrome_websocket(g_ctx.conn.target_id);
    if (ws_sock > 0) {
        g_ctx.conn.connected = 1;
        g_ctx.conn.last_activity = time(NULL);
        g_ctx.conn.reconnect_attempts = 0;  // Reset on success
        
        if (!verbose) { fprintf(stderr, " Reconnected!\n"); }
        else { cdp_log(CDP_LOG_INFO, "WS", "Successfully reconnected to WebSocket"); }
        
        // Re-enable Runtime if we're on a page endpoint
        if (strstr(g_ctx.conn.target_id, "page/")) {
            if (cdp_send_cmd("Runtime.enable", NULL) >= 0) {
                g_ctx.runtime.runtime_ready = 1;
            }
        }
        
        return ws_sock;
    }
    
    return -1;
}

/* Check WebSocket health with error recovery */
int check_ws_health(void) {
    if (ws_sock < 0) {
        DEBUG_LOG("WebSocket not connected");
        return -1;
    }
    
    // Check if socket is still valid using select
    fd_set read_fds, error_fds;
    struct timeval tv = {0, 0};  // Non-blocking check
    
    FD_ZERO(&read_fds);
    FD_ZERO(&error_fds);
    FD_SET(ws_sock, &read_fds);
    FD_SET(ws_sock, &error_fds);
    
    int ret = select(ws_sock + 1, &read_fds, NULL, &error_fds, &tv);
    
    if (ret < 0) {
        DEBUG_LOG("Socket select error: %s", strerror(errno));
        return -1;
    }
    
    if (FD_ISSET(ws_sock, &error_fds)) {
        DEBUG_LOG("Socket in error state");
        return -1;
    }
    
    // Socket appears healthy
    return 0;
}
