/*
 * mod_stream.c - Node.js-style Stream implementation for cosmorun
 */

#include "mod_stream.h"

/* ==================== Internal Buffer Implementation ==================== */

typedef struct {
    void *data;
    size_t length;
} stream_data_event_t;

typedef struct stream_buffer_chunk_t {
    void *data;
    size_t length;
    size_t offset;  // Read offset within this chunk
    struct stream_buffer_chunk_t *next;
} stream_buffer_chunk_t;

struct stream_buffer_t {
    stream_buffer_chunk_t *head;
    stream_buffer_chunk_t *tail;
    size_t total_length;  // Total buffered bytes
};

static stream_buffer_t* buffer_create(void) {
    stream_buffer_t *buf = (stream_buffer_t*)calloc(1, sizeof(stream_buffer_t));
    return buf;
}

static void buffer_destroy(stream_buffer_t *buf) {
    if (!buf) return;

    stream_buffer_chunk_t *chunk = buf->head;
    while (chunk) {
        stream_buffer_chunk_t *next = chunk->next;
        free(chunk->data);
        free(chunk);
        chunk = next;
    }
    free(buf);
}

static int buffer_push(stream_buffer_t *buf, const void *data, size_t length) {
    if (!buf || !data || length == 0) return -1;

    stream_buffer_chunk_t *chunk = (stream_buffer_chunk_t*)malloc(sizeof(stream_buffer_chunk_t));
    if (!chunk) return -1;

    chunk->data = malloc(length);
    if (!chunk->data) {
        free(chunk);
        return -1;
    }

    memcpy(chunk->data, data, length);
    chunk->length = length;
    chunk->offset = 0;
    chunk->next = NULL;

    if (buf->tail) {
        buf->tail->next = chunk;
    } else {
        buf->head = chunk;
    }
    buf->tail = chunk;
    buf->total_length += length;

    return 0;
}

static ssize_t buffer_read(stream_buffer_t *buf, void *dest, size_t size) {
    if (!buf || !dest || size == 0) return 0;

    size_t bytes_read = 0;
    char *out = (char*)dest;

    while (buf->head && bytes_read < size) {
        stream_buffer_chunk_t *chunk = buf->head;
        size_t available = chunk->length - chunk->offset;
        size_t to_read = (size - bytes_read < available) ? (size - bytes_read) : available;

        memcpy(out + bytes_read, (char*)chunk->data + chunk->offset, to_read);
        bytes_read += to_read;
        chunk->offset += to_read;
        buf->total_length -= to_read;

        // Remove exhausted chunk
        if (chunk->offset >= chunk->length) {
            buf->head = chunk->next;
            if (!buf->head) {
                buf->tail = NULL;
            }
            free(chunk->data);
            free(chunk);
        }
    }

    return bytes_read;
}

static size_t buffer_length(stream_buffer_t *buf) {
    return buf ? buf->total_length : 0;
}

static void buffer_clear(stream_buffer_t *buf) {
    if (!buf) return;

    stream_buffer_chunk_t *chunk = buf->head;
    while (chunk) {
        stream_buffer_chunk_t *next = chunk->next;
        free(chunk->data);
        free(chunk);
        chunk = next;
    }

    buf->head = NULL;
    buf->tail = NULL;
    buf->total_length = 0;
}

/* ==================== Event System Implementation ==================== */

void stream_on(stream_t *stream, stream_event_type_t event,
               stream_event_listener_t callback, void *userdata) {
    if (!stream || !callback) return;

    stream_listener_t *listener = (stream_listener_t*)malloc(sizeof(stream_listener_t));
    if (!listener) return;

    listener->event = event;
    listener->callback = callback;
    listener->userdata = userdata;
    listener->next = stream->listeners;
    stream->listeners = listener;
}

void stream_off(stream_t *stream, stream_event_type_t event,
                stream_event_listener_t callback) {
    if (!stream) return;

    stream_listener_t **current = &stream->listeners;
    while (*current) {
        stream_listener_t *listener = *current;
        if (listener->event == event && listener->callback == callback) {
            *current = listener->next;
            free(listener);
            return;
        }
        current = &listener->next;
    }
}

void stream_emit(stream_t *stream, stream_event_type_t event, void *data) {
    if (!stream) return;

    stream_listener_t *listener = stream->listeners;
    while (listener) {
        if (listener->event == event) {
            listener->callback(stream, data, listener->userdata);
        }
        listener = listener->next;
    }
}

/* ==================== Stream Creation/Destruction ==================== */

stream_t* stream_create(stream_type_t type, stream_options_t *options) {
    stream_t *stream = (stream_t*)calloc(1, sizeof(stream_t));
    if (!stream) return NULL;

    stream->type = type;
    stream->state = STREAM_STATE_PAUSED;

    // Initialize buffers based on type
    if (type & STREAM_TYPE_READABLE) {
        stream->read_buffer = buffer_create();
        if (!stream->read_buffer) {
            free(stream);
            return NULL;
        }
    }

    if (type & STREAM_TYPE_WRITABLE) {
        stream->write_buffer = buffer_create();
        if (!stream->write_buffer) {
            if (stream->read_buffer) buffer_destroy(stream->read_buffer);
            free(stream);
            return NULL;
        }
    }

    // Set options
    if (options) {
        stream->high_water_mark = options->high_water_mark;
        stream->object_mode = options->object_mode;
        stream->read_fn = options->read_fn;
        stream->write_fn = options->write_fn;
        stream->transform_fn = options->transform_fn;
        stream->flush_fn = options->flush_fn;
        stream->userdata = options->userdata;
    }

    if (stream->high_water_mark == 0) {
        stream->high_water_mark = STREAM_DEFAULT_HIGH_WATER_MARK;
    }

    // Initialize pipe array
    stream->pipes_capacity = 4;
    stream->pipes = (stream_t**)calloc(stream->pipes_capacity, sizeof(stream_t*));

    return stream;
}

void stream_destroy(stream_t *stream) {
    if (!stream) return;

    // Mark as destroyed
    stream->state |= STREAM_STATE_DESTROYED;

    // Emit close event
    stream_emit(stream, STREAM_EVENT_CLOSE, NULL);

    // Free listeners
    stream_listener_t *listener = stream->listeners;
    while (listener) {
        stream_listener_t *next = listener->next;
        free(listener);
        listener = next;
    }

    // Free buffers
    if (stream->read_buffer) buffer_destroy(stream->read_buffer);
    if (stream->write_buffer) buffer_destroy(stream->write_buffer);

    // Free pipes
    if (stream->pipes) free(stream->pipes);

    // Free error message
    if (stream->error_message) free(stream->error_message);

    free(stream);
}

/* ==================== Readable Stream Implementation ==================== */

int stream_push(stream_t *stream, const void *chunk, size_t length) {
    if (!stream || !(stream->type & STREAM_TYPE_READABLE)) return 0;
    if (stream->state & (STREAM_STATE_ENDED | STREAM_STATE_DESTROYED)) return 0;

    // NULL chunk signals end
    if (!chunk) {
        stream_end_readable(stream);
        return 0;
    }

    // Add to buffer
    if (buffer_push(stream->read_buffer, chunk, length) < 0) {
        return 0;
    }

    // If in flowing mode, emit data event
    if (stream->state & STREAM_STATE_FLOWING) {
        // Create event structure with data and length
        stream_data_event_t event = { (void*)chunk, length };
        stream_emit(stream, STREAM_EVENT_DATA, &event);
    }

    // Check backpressure
    if (buffer_length(stream->read_buffer) >= stream->high_water_mark) {
        return 0;  // Signal to stop pushing
    }

    return 1;  // Can push more
}

ssize_t stream_read(stream_t *stream, void *buffer, size_t size) {
    if (!stream || !buffer || !(stream->type & STREAM_TYPE_READABLE)) return -1;
    if (stream->state & STREAM_STATE_DESTROYED) return -1;

    // If no data and ended, return 0
    if ((stream->state & STREAM_STATE_ENDED) && buffer_length(stream->read_buffer) == 0) {
        return 0;
    }

    // Trigger read callback if buffer is low or empty (to fill it)
    if (stream->read_fn && buffer_length(stream->read_buffer) < stream->high_water_mark / 2) {
        stream->read_fn(stream, stream->high_water_mark, stream->userdata);
    }

    // Read from buffer
    size_t to_read = size;
    if (to_read == 0) {
        to_read = buffer_length(stream->read_buffer);
    }

    ssize_t bytes_read = buffer_read(stream->read_buffer, buffer, to_read);

    return bytes_read;
}

void stream_pause(stream_t *stream) {
    if (!stream || !(stream->type & STREAM_TYPE_READABLE)) return;
    stream->state &= ~STREAM_STATE_FLOWING;
    stream->state |= STREAM_STATE_PAUSED;
}

void stream_resume(stream_t *stream) {
    if (!stream || !(stream->type & STREAM_TYPE_READABLE)) return;
    stream->state &= ~STREAM_STATE_PAUSED;
    stream->state |= STREAM_STATE_FLOWING;

    // Emit buffered data
    while (buffer_length(stream->read_buffer) > 0) {
        char chunk[1024];
        ssize_t bytes = buffer_read(stream->read_buffer, chunk, sizeof(chunk));
        if (bytes > 0) {
            stream_data_event_t event = { chunk, (size_t)bytes };
            stream_emit(stream, STREAM_EVENT_DATA, &event);
        } else {
            break;
        }
    }

    // Trigger read callback
    if (stream->read_fn) {
        stream->read_fn(stream, stream->high_water_mark, stream->userdata);
    }
}

int stream_is_paused(stream_t *stream) {
    if (!stream) return 1;
    return (stream->state & STREAM_STATE_FLOWING) == 0;
}

void stream_end_readable(stream_t *stream) {
    if (!stream || !(stream->type & STREAM_TYPE_READABLE)) return;
    if (stream->state & STREAM_STATE_ENDED) return;

    stream->state |= STREAM_STATE_ENDED;
    stream_emit(stream, STREAM_EVENT_END, NULL);

    // End all piped destinations
    for (size_t i = 0; i < stream->pipes_count; i++) {
        if (stream->pipes[i]) {
            stream_end(stream->pipes[i], NULL, 0);
        }
    }
}

/* ==================== Writable Stream Implementation ==================== */

int stream_write(stream_t *stream, const void *chunk, size_t length) {
    if (!stream || !chunk || length == 0) return -1;
    if (!(stream->type & STREAM_TYPE_WRITABLE)) return -1;
    if (stream->state & (STREAM_STATE_FINISHED | STREAM_STATE_DESTROYED)) return -1;

    // For transform streams, apply transformation
    if (stream->type == STREAM_TYPE_TRANSFORM && stream->transform_fn) {
        stream->transform_fn(stream, chunk, length, stream->userdata);
        return 1;
    }

    // Call write callback if provided
    if (stream->write_fn) {
        int result = stream->write_fn(stream, chunk, length, stream->userdata);
        if (result < 0) {
            stream->state |= STREAM_STATE_ERROR;
            return -1;
        }
    } else {
        // Buffer the data
        if (buffer_push(stream->write_buffer, chunk, length) < 0) {
            return -1;
        }
    }

    // Check backpressure
    if (buffer_length(stream->write_buffer) >= stream->high_water_mark) {
        return 0;  // Signal backpressure
    }

    return 1;  // Can write more
}

void stream_end(stream_t *stream, const void *chunk, size_t length) {
    if (!stream || !(stream->type & STREAM_TYPE_WRITABLE)) return;
    if (stream->state & STREAM_STATE_FINISHED) return;

    // Write final chunk if provided
    if (chunk && length > 0) {
        stream_write(stream, chunk, length);
    }

    // For transform streams, call flush
    if (stream->type == STREAM_TYPE_TRANSFORM && stream->flush_fn) {
        stream->flush_fn(stream, stream->userdata);
    }

    stream->state |= STREAM_STATE_FINISHED;
    stream_emit(stream, STREAM_EVENT_FINISH, NULL);
}

int stream_is_writable(stream_t *stream) {
    if (!stream || !(stream->type & STREAM_TYPE_WRITABLE)) return 0;
    if (stream->state & (STREAM_STATE_FINISHED | STREAM_STATE_DESTROYED)) return 0;
    return 1;
}

/* ==================== Pipe Implementation ==================== */

static void pipe_data_handler(stream_t *source, void *data, void *userdata) {
    stream_t *dest = (stream_t*)userdata;
    if (!dest || !data) return;

    // Data is a stream_data_event_t structure
    stream_data_event_t *event = (stream_data_event_t*)data;
    stream_write(dest, event->data, event->length);
}

static void pipe_end_handler(stream_t *source, void *data, void *userdata) {
    stream_t *dest = (stream_t*)userdata;
    if (!dest) return;
    stream_end(dest, NULL, 0);
}

stream_t* stream_pipe(stream_t *source, stream_t *dest) {
    if (!source || !dest) return dest;
    if (!(source->type & STREAM_TYPE_READABLE)) return dest;
    if (!(dest->type & STREAM_TYPE_WRITABLE)) return dest;

    // Add to pipe list
    if (source->pipes_count >= source->pipes_capacity) {
        size_t new_capacity = source->pipes_capacity * 2;
        stream_t **new_pipes = (stream_t**)realloc(source->pipes,
                                                    new_capacity * sizeof(stream_t*));
        if (!new_pipes) return dest;
        source->pipes = new_pipes;
        source->pipes_capacity = new_capacity;
    }

    source->pipes[source->pipes_count++] = dest;

    // Emit pipe event
    stream_emit(source, STREAM_EVENT_PIPE, dest);

    // Set up automatic data forwarding
    stream_on(source, STREAM_EVENT_DATA, pipe_data_handler, dest);
    stream_on(source, STREAM_EVENT_END, pipe_end_handler, dest);

    // Resume source if paused
    if (stream_is_paused(source)) {
        stream_resume(source);
    }

    return dest;
}

void stream_unpipe(stream_t *source, stream_t *dest) {
    if (!source) return;

    if (!dest) {
        // Unpipe all
        for (size_t i = 0; i < source->pipes_count; i++) {
            stream_emit(source, STREAM_EVENT_UNPIPE, source->pipes[i]);
            stream_off(source, STREAM_EVENT_DATA, pipe_data_handler);
            stream_off(source, STREAM_EVENT_END, pipe_end_handler);
        }
        source->pipes_count = 0;
    } else {
        // Unpipe specific destination
        for (size_t i = 0; i < source->pipes_count; i++) {
            if (source->pipes[i] == dest) {
                stream_emit(source, STREAM_EVENT_UNPIPE, dest);
                // Remove from array
                for (size_t j = i; j < source->pipes_count - 1; j++) {
                    source->pipes[j] = source->pipes[j + 1];
                }
                source->pipes_count--;
                break;
            }
        }
    }
}

/* ==================== State Queries ==================== */

int stream_is_readable(stream_t *stream) {
    if (!stream || !(stream->type & STREAM_TYPE_READABLE)) return 0;
    if (stream->state & (STREAM_STATE_ENDED | STREAM_STATE_DESTROYED)) return 0;
    return 1;
}

int stream_is_ended(stream_t *stream) {
    return stream && (stream->state & STREAM_STATE_ENDED);
}

int stream_is_finished(stream_t *stream) {
    return stream && (stream->state & STREAM_STATE_FINISHED);
}

int stream_is_destroyed(stream_t *stream) {
    return stream && (stream->state & STREAM_STATE_DESTROYED);
}

const char* stream_get_error(stream_t *stream) {
    if (!stream || !(stream->state & STREAM_STATE_ERROR)) return NULL;
    return stream->error_message;
}

/* ==================== Utility Functions ==================== */

size_t stream_readable_length(stream_t *stream) {
    if (!stream || !stream->read_buffer) return 0;
    return buffer_length(stream->read_buffer);
}

size_t stream_writable_length(stream_t *stream) {
    if (!stream || !stream->write_buffer) return 0;
    return buffer_length(stream->write_buffer);
}

stream_t* stream_from_buffer(const void *data, size_t length) {
    stream_t *stream = stream_create(STREAM_TYPE_READABLE, NULL);
    if (!stream) return NULL;

    if (data && length > 0) {
        stream_push(stream, data, length);
        stream_push(stream, NULL, 0);  // Signal end
    }

    return stream;
}

stream_t* stream_to_buffer(void) {
    return stream_create(STREAM_TYPE_WRITABLE, NULL);
}

void* stream_get_buffer(stream_t *stream, size_t *out_length) {
    if (!stream || !stream->write_buffer) {
        if (out_length) *out_length = 0;
        return NULL;
    }

    size_t length = buffer_length(stream->write_buffer);
    if (length == 0) {
        if (out_length) *out_length = 0;
        return NULL;
    }

    void *buffer = malloc(length);
    if (!buffer) {
        if (out_length) *out_length = 0;
        return NULL;
    }

    ssize_t bytes_read = buffer_read(stream->write_buffer, buffer, length);
    if (out_length) *out_length = bytes_read;

    return buffer;
}
