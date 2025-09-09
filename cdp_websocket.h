/**
 * WebSocket Client Abstraction
 * Hides WebSocket protocol implementation details
 */

#ifndef CDP_WEBSOCKET_H
#define CDP_WEBSOCKET_H

#include <stddef.h>
#include <stdint.h>

typedef struct WebSocketClient WebSocketClient;
typedef struct WebSocketMessage WebSocketMessage;

/* WebSocket opcodes */
typedef enum {
    WS_OPCODE_CONTINUATION = 0x0,
    WS_OPCODE_TEXT = 0x1,
    WS_OPCODE_BINARY = 0x2,
    WS_OPCODE_CLOSE = 0x8,
    WS_OPCODE_PING = 0x9,
    WS_OPCODE_PONG = 0xA
} WebSocketOpcode;

/* WebSocket state */
typedef enum {
    WS_STATE_DISCONNECTED = 0,
    WS_STATE_CONNECTING,
    WS_STATE_CONNECTED,
    WS_STATE_CLOSING,
    WS_STATE_CLOSED
} WebSocketState;

/* WebSocket configuration */
typedef struct {
    const char* host;
    int port;
    const char* path;
    int timeout_ms;
    int enable_auto_ping;
    int ping_interval_ms;
    int max_frame_size;
} WebSocketConfig;

/* WebSocket message */
struct WebSocketMessage {
    WebSocketOpcode opcode;
    uint8_t* payload;
    size_t payload_len;
    int is_final;
    struct WebSocketMessage* next;  /* For fragmented messages */
};

/* WebSocket client interface */
struct WebSocketClient {
    /* Private implementation */
    void* priv;
    
    /* Public methods */
    int (*connect)(WebSocketClient* client, const WebSocketConfig* config);
    int (*disconnect)(WebSocketClient* client);
    int (*send_text)(WebSocketClient* client, const char* text);
    int (*send_binary)(WebSocketClient* client, const uint8_t* data, size_t len);
    WebSocketMessage* (*receive)(WebSocketClient* client, int timeout_ms);
    int (*send_ping)(WebSocketClient* client, const uint8_t* data, size_t len);
    int (*send_pong)(WebSocketClient* client, const uint8_t* data, size_t len);
    WebSocketState (*get_state)(WebSocketClient* client);
    int (*is_connected)(WebSocketClient* client);
    const char* (*get_error)(WebSocketClient* client);
    
    /* Callbacks */
    void (*on_connect)(WebSocketClient* client, void* userdata);
    void (*on_disconnect)(WebSocketClient* client, void* userdata);
    void (*on_message)(WebSocketClient* client, WebSocketMessage* msg, void* userdata);
    void (*on_error)(WebSocketClient* client, const char* error, void* userdata);
    void* callback_userdata;
};

/* Client creation and destruction */
WebSocketClient* ws_client_create(void);
void ws_client_destroy(WebSocketClient* client);

/* Connection management */
int ws_client_connect(WebSocketClient* client, const char* url);
int ws_client_connect_ex(WebSocketClient* client, const WebSocketConfig* config);
int ws_client_disconnect(WebSocketClient* client);
int ws_client_reconnect(WebSocketClient* client);

/* Message operations */
int ws_client_send_text(WebSocketClient* client, const char* text);
int ws_client_send_binary(WebSocketClient* client, const uint8_t* data, size_t len);
char* ws_client_receive_text(WebSocketClient* client, int timeout_ms);
WebSocketMessage* ws_client_receive(WebSocketClient* client, int timeout_ms);
void ws_message_free(WebSocketMessage* msg);

/* Utility functions */
int ws_client_set_timeout(WebSocketClient* client, int timeout_ms);
int ws_client_enable_auto_ping(WebSocketClient* client, int interval_ms);
WebSocketState ws_client_get_state(WebSocketClient* client);
int ws_client_is_connected(WebSocketClient* client);
const char* ws_client_get_last_error(WebSocketClient* client);

/* Low-level frame operations (usually not needed) */
int ws_frame_create(uint8_t** frame, size_t* frame_len, 
                    WebSocketOpcode opcode, const uint8_t* payload, size_t payload_len,
                    int use_mask);
int ws_frame_parse(const uint8_t* frame, size_t frame_len,
                   WebSocketOpcode* opcode, uint8_t** payload, size_t* payload_len);

#endif /* CDP_WEBSOCKET_H */