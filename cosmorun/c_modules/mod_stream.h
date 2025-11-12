#ifndef MOD_STREAM_H
#define MOD_STREAM_H

/*
 * mod_stream - Node.js-style Stream module for cosmorun
 *
 * Provides Readable, Writable, Duplex, and Transform stream types
 * with events, backpressure, and pipe support.
 *
 * Stream Types:
 * - Readable: Data source (e.g., file read, network receive)
 * - Writable: Data sink (e.g., file write, network send)
 * - Duplex: Both readable and writable (e.g., TCP socket)
 * - Transform: Duplex that transforms data (e.g., compression)
 *
 * Events:
 * - 'data': Emitted when data is available (flowing mode)
 * - 'end': Emitted when no more data (readable)
 * - 'finish': Emitted when all data written (writable)
 * - 'error': Emitted on errors
 * - 'drain': Emitted when write buffer drains (backpressure)
 * - 'pipe': Emitted when stream is piped to
 * - 'unpipe': Emitted when stream is unpiped
 */

#include "src/cosmo_libc.h"

/* ==================== Forward Declarations ==================== */

typedef struct stream_t stream_t;
typedef struct stream_buffer_t stream_buffer_t;

/* ==================== Stream Types ==================== */

typedef enum {
    STREAM_TYPE_READABLE = 0x01,
    STREAM_TYPE_WRITABLE = 0x02,
    STREAM_TYPE_DUPLEX = 0x03,    // READABLE | WRITABLE
    STREAM_TYPE_TRANSFORM = 0x07  // DUPLEX | special flag (0x03 | 0x04)
} stream_type_t;

/* ==================== Stream State ==================== */

typedef enum {
    STREAM_STATE_PAUSED = 0,
    STREAM_STATE_FLOWING = 1,
    STREAM_STATE_ENDED = 2,
    STREAM_STATE_FINISHED = 4,
    STREAM_STATE_ERROR = 8,
    STREAM_STATE_DESTROYED = 16
} stream_state_t;

/* ==================== Event System ==================== */

typedef enum {
    STREAM_EVENT_DATA = 0,
    STREAM_EVENT_END = 1,
    STREAM_EVENT_FINISH = 2,
    STREAM_EVENT_ERROR = 3,
    STREAM_EVENT_DRAIN = 4,
    STREAM_EVENT_PIPE = 5,
    STREAM_EVENT_UNPIPE = 6,
    STREAM_EVENT_CLOSE = 7
} stream_event_type_t;

/* Event listener callback */
typedef void (*stream_event_listener_t)(stream_t *stream, void *data, void *userdata);

typedef struct stream_listener_t {
    stream_event_type_t event;
    stream_event_listener_t callback;
    void *userdata;
    struct stream_listener_t *next;
} stream_listener_t;

/* ==================== Callback Types ==================== */

/**
 * Read callback for Readable streams
 * Called when stream needs data (pull-based)
 * @param stream The stream instance
 * @param size Suggested read size (0 = any size)
 * @param userdata User-provided data
 */
typedef void (*stream_read_callback_t)(stream_t *stream, size_t size, void *userdata);

/**
 * Write callback for Writable streams
 * Called when data is written to stream
 * @param stream The stream instance
 * @param chunk Data chunk
 * @param length Chunk length
 * @param userdata User-provided data
 * @return 0 on success, -1 on error
 */
typedef int (*stream_write_callback_t)(stream_t *stream, const void *chunk, size_t length, void *userdata);

/**
 * Transform callback for Transform streams
 * Called to transform data chunks
 * @param stream The stream instance
 * @param chunk Input data chunk
 * @param length Input chunk length
 * @param userdata User-provided data
 */
typedef void (*stream_transform_callback_t)(stream_t *stream, const void *chunk, size_t length, void *userdata);

/**
 * Flush callback for Transform streams
 * Called when stream is ending, to flush any buffered data
 * @param stream The stream instance
 * @param userdata User-provided data
 */
typedef void (*stream_flush_callback_t)(stream_t *stream, void *userdata);

/* ==================== Stream Options ==================== */

typedef struct {
    size_t high_water_mark;         // Backpressure threshold (default: 16KB)
    int object_mode;                // Object mode (default: 0)
    int encoding;                   // Text encoding (default: NULL/binary)
    stream_read_callback_t read_fn; // Read callback
    stream_write_callback_t write_fn; // Write callback
    stream_transform_callback_t transform_fn; // Transform callback
    stream_flush_callback_t flush_fn; // Flush callback
    void *userdata;                 // User-provided data
} stream_options_t;

/* Default options */
#define STREAM_DEFAULT_HIGH_WATER_MARK (16 * 1024)  // 16KB

/* ==================== Stream Structure ==================== */

struct stream_t {
    stream_type_t type;             // Stream type
    unsigned int state;             // Stream state (bitflags)

    // Event system
    stream_listener_t *listeners;   // Event listeners

    // Internal buffers
    stream_buffer_t *read_buffer;   // Readable buffer
    stream_buffer_t *write_buffer;  // Writable buffer

    // Options
    size_t high_water_mark;         // Backpressure threshold
    int object_mode;                // Object mode flag

    // Callbacks
    stream_read_callback_t read_fn;
    stream_write_callback_t write_fn;
    stream_transform_callback_t transform_fn;
    stream_flush_callback_t flush_fn;
    void *userdata;

    // Pipe chain
    stream_t **pipes;               // Array of piped destinations
    size_t pipes_count;
    size_t pipes_capacity;

    // Error tracking
    char *error_message;
};

/* ==================== Core API ==================== */

/**
 * Create a new stream
 * @param type Stream type
 * @param options Stream options (NULL for defaults)
 * @return New stream instance
 */
stream_t* stream_create(stream_type_t type, stream_options_t *options);

/**
 * Destroy stream and free resources
 * @param stream Stream instance
 */
void stream_destroy(stream_t *stream);

/* ==================== Readable Stream API ==================== */

/**
 * Read data from stream
 * @param stream Stream instance
 * @param buffer Output buffer
 * @param size Maximum bytes to read (0 = all available)
 * @return Bytes read, 0 if no data, -1 on error
 */
ssize_t stream_read(stream_t *stream, void *buffer, size_t size);

/**
 * Push data into readable stream (producer side)
 * @param stream Stream instance
 * @param chunk Data chunk (NULL to signal end)
 * @param length Chunk length
 * @return 1 if can push more, 0 if should stop (backpressure)
 */
int stream_push(stream_t *stream, const void *chunk, size_t length);

/**
 * Pause data flow (switch to paused mode)
 * @param stream Stream instance
 */
void stream_pause(stream_t *stream);

/**
 * Resume data flow (switch to flowing mode)
 * @param stream Stream instance
 */
void stream_resume(stream_t *stream);

/**
 * Check if stream is paused
 * @param stream Stream instance
 * @return 1 if paused, 0 if flowing
 */
int stream_is_paused(stream_t *stream);

/**
 * Signal end of readable stream
 * @param stream Stream instance
 */
void stream_end_readable(stream_t *stream);

/* ==================== Writable Stream API ==================== */

/**
 * Write data to stream
 * @param stream Stream instance
 * @param chunk Data chunk
 * @param length Chunk length
 * @return 1 if can write more, 0 if should wait (backpressure), -1 on error
 */
int stream_write(stream_t *stream, const void *chunk, size_t length);

/**
 * End writable stream (optionally write final chunk)
 * @param stream Stream instance
 * @param chunk Optional final chunk (can be NULL)
 * @param length Final chunk length (0 if chunk is NULL)
 */
void stream_end(stream_t *stream, const void *chunk, size_t length);

/**
 * Check if stream is writable
 * @param stream Stream instance
 * @return 1 if writable, 0 otherwise
 */
int stream_is_writable(stream_t *stream);

/* ==================== Pipe API ==================== */

/**
 * Pipe readable stream to writable stream
 * @param source Readable stream
 * @param dest Writable stream
 * @return dest stream (for chaining)
 */
stream_t* stream_pipe(stream_t *source, stream_t *dest);

/**
 * Unpipe stream
 * @param source Source stream
 * @param dest Destination stream (NULL to unpipe all)
 */
void stream_unpipe(stream_t *source, stream_t *dest);

/* ==================== Event API ==================== */

/**
 * Add event listener
 * @param stream Stream instance
 * @param event Event type
 * @param callback Event callback
 * @param userdata User data for callback
 */
void stream_on(stream_t *stream, stream_event_type_t event,
               stream_event_listener_t callback, void *userdata);

/**
 * Remove event listener
 * @param stream Stream instance
 * @param event Event type
 * @param callback Event callback to remove
 */
void stream_off(stream_t *stream, stream_event_type_t event,
                stream_event_listener_t callback);

/**
 * Emit event (internal use)
 * @param stream Stream instance
 * @param event Event type
 * @param data Event data
 */
void stream_emit(stream_t *stream, stream_event_type_t event, void *data);

/* ==================== State Queries ==================== */

/**
 * Check if stream is readable
 * @param stream Stream instance
 * @return 1 if readable, 0 otherwise
 */
int stream_is_readable(stream_t *stream);

/**
 * Check if stream has ended
 * @param stream Stream instance
 * @return 1 if ended, 0 otherwise
 */
int stream_is_ended(stream_t *stream);

/**
 * Check if stream has finished
 * @param stream Stream instance
 * @return 1 if finished, 0 otherwise
 */
int stream_is_finished(stream_t *stream);

/**
 * Check if stream is destroyed
 * @param stream Stream instance
 * @return 1 if destroyed, 0 otherwise
 */
int stream_is_destroyed(stream_t *stream);

/**
 * Get error message if stream is in error state
 * @param stream Stream instance
 * @return Error message or NULL
 */
const char* stream_get_error(stream_t *stream);

/* ==================== Utility Functions ==================== */

/**
 * Get available bytes in read buffer
 * @param stream Stream instance
 * @return Number of bytes available
 */
size_t stream_readable_length(stream_t *stream);

/**
 * Get bytes in write buffer
 * @param stream Stream instance
 * @return Number of bytes buffered
 */
size_t stream_writable_length(stream_t *stream);

/**
 * Create readable stream with data
 * @param data Data buffer
 * @param length Data length
 * @return New readable stream
 */
stream_t* stream_from_buffer(const void *data, size_t length);

/**
 * Create writable stream that collects to buffer
 * @return New writable stream
 */
stream_t* stream_to_buffer(void);

/**
 * Get collected buffer from stream_to_buffer()
 * @param stream Stream instance
 * @param out_length Output length pointer
 * @return Buffer data (must be freed by caller)
 */
void* stream_get_buffer(stream_t *stream, size_t *out_length);

#endif /* MOD_STREAM_H */
