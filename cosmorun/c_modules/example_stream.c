/*
 * example_stream.c - Demonstration of mod_stream usage
 *
 * This example showcases various Stream patterns:
 * - Readable streams (data sources)
 * - Writable streams (data sinks)
 * - Transform streams (data transformation)
 * - Pipe chains (composable streams)
 * - Event handling
 * - Backpressure management
 */

#include "src/cosmo_libc.h"
#include "mod_stream.c"

/* ==================== Example 1: Simple Readable Stream ==================== */

void example_readable_stream(void) {
    printf("\n=== Example 1: Readable Stream ===\n");

    // Create a stream from a buffer
    const char *message = "Hello from readable stream!";
    stream_t *stream = stream_from_buffer(message, strlen(message));

    // Read data from stream
    char buffer[100] = {0};
    ssize_t bytes = stream_read(stream, buffer, sizeof(buffer));

    printf("Read %zd bytes: %s\n", bytes, buffer);

    stream_destroy(stream);
}

/* ==================== Example 2: Custom Readable Stream with Push ==================== */

static void custom_read_callback(stream_t *stream, size_t size, void *userdata) {
    int *counter = (int*)userdata;

    if (*counter >= 5) {
        // End stream after 5 chunks
        stream_push(stream, NULL, 0);
        return;
    }

    // Generate and push data
    char chunk[32];
    snprintf(chunk, sizeof(chunk), "Chunk #%d\n", (*counter)++);
    stream_push(stream, chunk, strlen(chunk));
}

void example_custom_readable(void) {
    printf("\n=== Example 2: Custom Readable Stream ===\n");

    int counter = 0;
    stream_options_t opts = {0};
    opts.read_fn = custom_read_callback;
    opts.userdata = &counter;

    stream_t *stream = stream_create(STREAM_TYPE_READABLE, &opts);

    // Read all data
    char buffer[256] = {0};
    while (!stream_is_ended(stream)) {
        char chunk[64];
        ssize_t bytes = stream_read(stream, chunk, sizeof(chunk));
        if (bytes > 0) {
            strncat(buffer, chunk, bytes);
        }
    }

    printf("Read from custom stream:\n%s", buffer);

    stream_destroy(stream);
}

/* ==================== Example 3: Writable Stream ==================== */

static int file_write_callback(stream_t *stream, const void *chunk, size_t length, void *userdata) {
    // In a real implementation, this would write to a file
    // For demo, just print
    printf("Writing %zu bytes: %.*s", length, (int)length, (char*)chunk);
    return 0;
}

void example_writable_stream(void) {
    printf("\n=== Example 3: Writable Stream ===\n");

    stream_options_t opts = {0};
    opts.write_fn = file_write_callback;

    stream_t *stream = stream_create(STREAM_TYPE_WRITABLE, &opts);

    // Write data
    stream_write(stream, "Line 1\n", 7);
    stream_write(stream, "Line 2\n", 7);
    stream_write(stream, "Line 3\n", 7);

    // End stream
    stream_end(stream, NULL, 0);

    stream_destroy(stream);
}

/* ==================== Example 4: Transform Stream ==================== */

// Transform: ROT13 encoding
static void rot13_transform(stream_t *stream, const void *chunk, size_t length, void *userdata) {
    char *output = (char*)malloc(length);
    if (!output) return;

    const char *input = (const char*)chunk;
    for (size_t i = 0; i < length; i++) {
        char c = input[i];
        if (c >= 'a' && c <= 'z') {
            output[i] = ((c - 'a' + 13) % 26) + 'a';
        } else if (c >= 'A' && c <= 'Z') {
            output[i] = ((c - 'A' + 13) % 26) + 'A';
        } else {
            output[i] = c;
        }
    }

    stream_push(stream, output, length);
    free(output);
}

void example_transform_stream(void) {
    printf("\n=== Example 4: Transform Stream (ROT13) ===\n");

    stream_options_t opts = {0};
    opts.transform_fn = rot13_transform;

    stream_t *transform = stream_create(STREAM_TYPE_TRANSFORM, &opts);

    // Write plain text
    const char *plain = "Hello World!";
    printf("Input:  %s\n", plain);
    stream_write(transform, plain, strlen(plain));

    // Read encoded text
    char buffer[100] = {0};
    ssize_t bytes = stream_read(transform, buffer, sizeof(buffer));
    printf("Output: %.*s\n", (int)bytes, buffer);

    // Write encoded text back
    stream_write(transform, buffer, bytes);

    // Read decoded text
    char decoded[100] = {0};
    bytes = stream_read(transform, decoded, sizeof(decoded));
    printf("Decode: %.*s\n", (int)bytes, decoded);

    stream_destroy(transform);
}

/* ==================== Example 5: Pipe Chain ==================== */

static int uppercase_count = 0;

static void uppercase_transform(stream_t *stream, const void *chunk, size_t length, void *userdata) {
    char *output = (char*)malloc(length);
    if (!output) return;

    const char *input = (const char*)chunk;
    for (size_t i = 0; i < length; i++) {
        char c = input[i];
        output[i] = (c >= 'a' && c <= 'z') ? (c - 32) : c;
    }

    stream_push(stream, output, length);
    free(output);
    uppercase_count++;
}

static void exclaim_transform(stream_t *stream, const void *chunk, size_t length, void *userdata) {
    // Add exclamation marks
    size_t new_len = length + 3;
    char *output = (char*)malloc(new_len);
    if (!output) return;

    memcpy(output, chunk, length);
    output[length] = '!';
    output[length + 1] = '!';
    output[length + 2] = '!';

    stream_push(stream, output, new_len);
    free(output);
}

void example_pipe_chain(void) {
    printf("\n=== Example 5: Pipe Chain ===\n");

    // Create source
    stream_t *source = stream_from_buffer("hello stream", 12);

    // Create transform: uppercase
    stream_options_t opts1 = {0};
    opts1.transform_fn = uppercase_transform;
    stream_t *upper = stream_create(STREAM_TYPE_TRANSFORM, &opts1);

    // Create transform: add exclamation
    stream_options_t opts2 = {0};
    opts2.transform_fn = exclaim_transform;
    stream_t *exclaim = stream_create(STREAM_TYPE_TRANSFORM, &opts2);

    // Create destination
    stream_t *dest = stream_to_buffer();

    // Build pipe chain: source -> upper -> exclaim -> dest
    stream_pipe(source, upper);
    stream_pipe(upper, exclaim);
    stream_pipe(exclaim, dest);

    // Resume source to start flowing
    stream_resume(source);

    // Get result
    size_t result_len = 0;
    void *result = stream_get_buffer(dest, &result_len);
    if (result) {
        printf("Result: %.*s\n", (int)result_len, (char*)result);
        free(result);
    }

    stream_destroy(source);
    stream_destroy(upper);
    stream_destroy(exclaim);
    stream_destroy(dest);
}

/* ==================== Example 6: Event Handling ==================== */

static void data_handler(stream_t *stream, void *data, void *userdata) {
    printf("  [DATA event] Received data\n");
}

static void end_handler(stream_t *stream, void *data, void *userdata) {
    printf("  [END event] Stream ended\n");
}

static void finish_handler(stream_t *stream, void *data, void *userdata) {
    printf("  [FINISH event] Stream finished\n");
}

void example_events(void) {
    printf("\n=== Example 6: Event Handling ===\n");

    // Readable stream with events
    stream_t *readable = stream_create(STREAM_TYPE_READABLE, NULL);
    stream_on(readable, STREAM_EVENT_DATA, data_handler, NULL);
    stream_on(readable, STREAM_EVENT_END, end_handler, NULL);

    printf("Pushing data to readable stream:\n");
    stream_push(readable, "data1", 5);
    stream_push(readable, "data2", 5);
    stream_push(readable, NULL, 0);  // End

    stream_destroy(readable);

    // Writable stream with events
    stream_t *writable = stream_create(STREAM_TYPE_WRITABLE, NULL);
    stream_on(writable, STREAM_EVENT_FINISH, finish_handler, NULL);

    printf("\nEnding writable stream:\n");
    stream_end(writable, NULL, 0);

    stream_destroy(writable);
}

/* ==================== Example 7: Backpressure ==================== */

void example_backpressure(void) {
    printf("\n=== Example 7: Backpressure ===\n");

    stream_options_t opts = {0};
    opts.high_water_mark = 32;  // Small buffer

    stream_t *stream = stream_create(STREAM_TYPE_READABLE, &opts);

    // Push data and observe backpressure
    char chunk[16];
    memset(chunk, 'X', sizeof(chunk));

    for (int i = 0; i < 5; i++) {
        int can_push = stream_push(stream, chunk, sizeof(chunk));
        printf("Push #%d: %s (buffered: %zu bytes)\n",
               i + 1,
               can_push ? "OK" : "BACKPRESSURE",
               stream_readable_length(stream));

        if (!can_push) {
            printf("  Draining buffer...\n");
            char drain[64];
            stream_read(stream, drain, sizeof(drain));
            printf("  After drain: %zu bytes buffered\n",
                   stream_readable_length(stream));
        }
    }

    stream_destroy(stream);
}

/* ==================== Example 8: Pause and Resume ==================== */

static int flowing_data_count = 0;

static void flowing_data_handler(stream_t *stream, void *data, void *userdata) {
    flowing_data_count++;
    printf("  [DATA] Chunk #%d received\n", flowing_data_count);
}

void example_pause_resume(void) {
    printf("\n=== Example 8: Pause and Resume ===\n");

    flowing_data_count = 0;
    stream_t *stream = stream_create(STREAM_TYPE_READABLE, NULL);
    stream_on(stream, STREAM_EVENT_DATA, flowing_data_handler, NULL);

    printf("Stream starts in PAUSED mode\n");
    printf("Is paused: %s\n", stream_is_paused(stream) ? "YES" : "NO");

    printf("\nPushing data while paused:\n");
    stream_push(stream, "chunk1", 6);
    stream_push(stream, "chunk2", 6);
    printf("  Data events emitted: %d (should be 0)\n", flowing_data_count);

    printf("\nResuming stream (enters FLOWING mode):\n");
    stream_resume(stream);
    printf("Is paused: %s\n", stream_is_paused(stream) ? "YES" : "NO");

    printf("\nPausing again:\n");
    stream_pause(stream);
    printf("Is paused: %s\n", stream_is_paused(stream) ? "YES" : "NO");

    stream_destroy(stream);
}

/* ==================== Main ==================== */

int main(void) {
    printf("╔══════════════════════════════════════════════╗\n");
    printf("║   mod_stream - Node.js Stream API Demo      ║\n");
    printf("╚══════════════════════════════════════════════╝\n");

    example_readable_stream();
    example_custom_readable();
    example_writable_stream();
    example_transform_stream();
    example_pipe_chain();
    example_events();
    example_backpressure();
    example_pause_resume();

    printf("\n╔══════════════════════════════════════════════╗\n");
    printf("║   All examples completed successfully!       ║\n");
    printf("╚══════════════════════════════════════════════╝\n");

    return 0;
}
