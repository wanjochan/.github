#ifndef COSMORUN_READLINE_H
#define COSMORUN_READLINE_H

/*
 * mod_readline - Command-line editing and history module for cosmorun
 *
 * Provides Node.js readline-style API for interactive command-line interfaces:
 * - Prompt display and line reading
 * - History management (add/get/clear)
 * - History persistence (save/load to file)
 * - Basic line editing (backspace, delete)
 * - Signal handling (Ctrl-C, Ctrl-D)
 * - UTF-8 support via mod_std
 *
 * Simple implementation using fgets() for maximum portability.
 * Advanced features (arrow keys, cursor movement) can be added later with termios.
 */

#include "mod_std.h"
#include "mod_events.h"

/* ==================== Types ==================== */

/* Readline interface structure */
typedef struct {
    char *prompt;                   // Current prompt string
    std_vector_t *history;          // History storage (vector of std_string_t*)
    size_t history_max_size;        // Maximum history entries (0 = unlimited)
    int closed;                     // Whether interface is closed
    event_emitter_t *emitter;       // Event emitter for 'line', 'close' events
    void *userdata;                 // User context data

    // Test mode support (for automated testing)
    int test_mode;                  // If 1, read from test_buffer instead of stdin
    const char **test_inputs;       // Array of test input strings
    int test_input_index;           // Current index in test_inputs
    int test_input_count;           // Total number of test inputs
} readline_t;

/* ==================== Core API ==================== */

/* Create a new readline interface
 * @param prompt: Initial prompt string (can be NULL for no prompt)
 * @return: New readline interface or NULL on allocation failure
 */
readline_t* readline_create(const char *prompt);

/* Read a line from input (blocking)
 * @param rl: Readline interface
 * @return: Newly allocated string with the line (without newline), or NULL on EOF/error
 *          Caller must free() the returned string
 *
 * Behavior:
 * - Displays prompt if set
 * - Reads until newline or EOF
 * - Trims trailing whitespace
 * - Returns NULL on Ctrl-D (EOF) or error
 * - Emits 'line' event if event emitter is set
 */
char* readline_read(readline_t *rl);

/* Set the prompt string
 * @param rl: Readline interface
 * @param prompt: New prompt string (can be NULL)
 */
void readline_set_prompt(readline_t *rl, const char *prompt);

/* Get the current prompt string
 * @param rl: Readline interface
 * @return: Current prompt string (can be NULL)
 */
const char* readline_get_prompt(readline_t *rl);

/* Close the readline interface
 * @param rl: Readline interface
 *
 * Marks the interface as closed. Further reads will return NULL.
 * Emits 'close' event if event emitter is set.
 */
void readline_close(readline_t *rl);

/* Free readline interface and all associated resources
 * @param rl: Readline interface
 */
void readline_free(readline_t *rl);

/* ==================== History Management ==================== */

/* Add a line to history
 * @param rl: Readline interface
 * @param line: Line to add (will be copied)
 * @return: 0 on success, -1 on error
 *
 * Behavior:
 * - Empty lines are not added
 * - Duplicate consecutive lines are not added
 * - Respects history_max_size limit
 */
int readline_history_add(readline_t *rl, const char *line);

/* Get a line from history by index
 * @param rl: Readline interface
 * @param index: History index (0 = oldest, size-1 = newest)
 * @return: History line string, or NULL if index out of bounds
 *          String is owned by history, do not free
 */
const char* readline_history_get(readline_t *rl, int index);

/* Get the number of lines in history
 * @param rl: Readline interface
 * @return: Number of history entries
 */
size_t readline_history_size(readline_t *rl);

/* Clear all history
 * @param rl: Readline interface
 */
void readline_history_clear(readline_t *rl);

/* Set maximum history size
 * @param rl: Readline interface
 * @param max_size: Maximum number of history entries (0 = unlimited)
 *
 * If current history exceeds new limit, oldest entries are removed.
 */
void readline_history_set_max_size(readline_t *rl, size_t max_size);

/* ==================== History Persistence ==================== */

/* Save history to a file
 * @param rl: Readline interface
 * @param filename: Path to history file
 * @return: 0 on success, -1 on error
 *
 * Format: Plain text, one line per entry, UTF-8 encoded
 */
int readline_history_save(readline_t *rl, const char *filename);

/* Load history from a file
 * @param rl: Readline interface
 * @param filename: Path to history file
 * @return: Number of lines loaded, or -1 on error
 *
 * Behavior:
 * - Appends to existing history
 * - Skips empty lines
 * - Respects history_max_size limit
 */
int readline_history_load(readline_t *rl, const char *filename);

/* ==================== Event API ==================== */

/* Set event emitter for readline interface
 * @param rl: Readline interface
 * @param emitter: Event emitter (takes ownership, will be freed with readline)
 *
 * Events emitted:
 * - 'line': When a line is read (data: char* line)
 * - 'close': When interface is closed (data: NULL)
 */
void readline_set_emitter(readline_t *rl, event_emitter_t *emitter);

/* ==================== Test Mode API ==================== */

/* Create readline interface in test mode (for automated testing)
 * @param prompt: Initial prompt string
 * @param test_inputs: Array of input strings to simulate
 * @param test_input_count: Number of inputs in the array
 * @return: New readline interface in test mode
 *
 * In test mode, readline_read() returns strings from test_inputs array
 * instead of reading from stdin. Returns NULL after all inputs are consumed.
 */
readline_t* readline_create_test(const char *prompt, const char **test_inputs, int test_input_count);

#endif /* COSMORUN_READLINE_H */
