/*
 * mod_readline.c - Command-line editing and history module implementation
 */

#include "mod_readline.h"
#include "mod_error_impl.h"

/* ==================== Helper Functions ==================== */

/* Trim trailing whitespace from a string (in-place) */
static void trim_trailing_whitespace(char *str) {
    if (!str) return;

    size_t len = strlen(str);
    while (len > 0 && (str[len-1] == ' ' || str[len-1] == '\t' ||
                       str[len-1] == '\n' || str[len-1] == '\r')) {
        str[--len] = '\0';
    }
}

/* Check if a string is empty or contains only whitespace */
static int is_empty_or_whitespace(const char *str) {
    if (!str) return 1;
    while (*str) {
        if (*str != ' ' && *str != '\t' && *str != '\n' && *str != '\r') {
            return 0;
        }
        str++;
    }
    return 1;
}

/* ==================== Core API Implementation ==================== */

readline_t* readline_create(const char *prompt) {
    readline_t *rl = (readline_t*)malloc(sizeof(readline_t));
    if (!rl) {
        COSMORUN_ERROR_NULL(COSMORUN_ERR_OUT_OF_MEMORY, "Failed to allocate readline context");
    }

    rl->prompt = prompt ? strdup(prompt) : NULL;
    rl->history = std_vector_new();
    rl->history_max_size = 0;  // Unlimited by default
    rl->closed = 0;
    rl->emitter = NULL;
    rl->userdata = NULL;
    rl->test_mode = 0;
    rl->test_inputs = NULL;
    rl->test_input_index = 0;
    rl->test_input_count = 0;

    if (!rl->history) {
        free(rl->prompt);
        free(rl);
        COSMORUN_ERROR_NULL(COSMORUN_ERR_OUT_OF_MEMORY, "Failed to allocate history vector");
    }

    return rl;
}

char* readline_read(readline_t *rl) {
    if (!rl) {
        COSMORUN_ERROR_NULL(COSMORUN_ERR_NULL_POINTER, "readline context is NULL");
    }
    if (rl->closed) {
        COSMORUN_ERROR_NULL(COSMORUN_ERR_INVALID_ARG, "readline context is closed");
    }

    // Test mode: return next test input
    if (rl->test_mode) {
        if (rl->test_input_index >= rl->test_input_count) {
            return NULL;  // EOF in test mode
        }
        const char *input = rl->test_inputs[rl->test_input_index++];
        char *result = strdup(input);

        // Emit 'line' event if emitter is set
        if (rl->emitter) {
            event_emit(rl->emitter, "line", result);
        }

        return result;
    }

    // Display prompt
    if (rl->prompt) {
        printf("%s", rl->prompt);
        fflush(stdout);
    }

    // Read line from stdin
    char buffer[4096];
    if (!fgets(buffer, sizeof(buffer), stdin)) {
        // EOF or error
        return NULL;
    }

    // Trim trailing whitespace (including newline)
    trim_trailing_whitespace(buffer);

    // Create result string
    char *result = strdup(buffer);

    // Emit 'line' event if emitter is set
    if (rl->emitter && result) {
        event_emit(rl->emitter, "line", result);
    }

    return result;
}

void readline_set_prompt(readline_t *rl, const char *prompt) {
    if (!rl) return;

    if (rl->prompt) {
        free(rl->prompt);
    }
    rl->prompt = prompt ? strdup(prompt) : NULL;
}

const char* readline_get_prompt(readline_t *rl) {
    return rl ? rl->prompt : NULL;
}

void readline_close(readline_t *rl) {
    if (!rl || rl->closed) return;

    rl->closed = 1;

    // Emit 'close' event if emitter is set
    if (rl->emitter) {
        event_emit(rl->emitter, "close", NULL);
    }
}

void readline_free(readline_t *rl) {
    if (!rl) return;

    // Free prompt
    if (rl->prompt) {
        free(rl->prompt);
    }

    // Free history
    if (rl->history) {
        for (size_t i = 0; i < std_vector_len(rl->history); i++) {
            std_string_t *entry = (std_string_t*)std_vector_get(rl->history, i);
            std_string_free(entry);
        }
        std_vector_free(rl->history);
    }

    // Free event emitter
    if (rl->emitter) {
        event_emitter_free(rl->emitter);
    }

    free(rl);
}

/* ==================== History Management ==================== */

int readline_history_add(readline_t *rl, const char *line) {
    if (!rl || !rl->history) {
        COSMORUN_ERROR_RETURN(COSMORUN_ERR_NULL_POINTER, "readline context or history is NULL", -1);
    }

    // Skip empty lines
    if (is_empty_or_whitespace(line)) {
        return 0;
    }

    // Check for duplicate consecutive entry
    size_t history_len = std_vector_len(rl->history);
    if (history_len > 0) {
        std_string_t *last = (std_string_t*)std_vector_get(rl->history, history_len - 1);
        if (last && strcmp(std_string_cstr(last), line) == 0) {
            return 0;  // Skip duplicate
        }
    }

    // Create new history entry
    std_string_t *entry = std_string_new(line);
    if (!entry) {
        COSMORUN_ERROR_RETURN(COSMORUN_ERR_OUT_OF_MEMORY, "Failed to allocate history entry", -1);
    }

    // Add to history
    std_vector_push(rl->history, entry);

    // Enforce max size limit
    if (rl->history_max_size > 0) {
        while (std_vector_len(rl->history) > rl->history_max_size) {
            std_string_t *oldest = (std_string_t*)std_vector_get(rl->history, 0);
            std_string_free(oldest);

            // Shift all entries down
            for (size_t i = 0; i < std_vector_len(rl->history) - 1; i++) {
                std_vector_set(rl->history, i, std_vector_get(rl->history, i + 1));
            }
            rl->history->len--;
        }
    }

    return 0;
}

const char* readline_history_get(readline_t *rl, int index) {
    if (!rl || !rl->history) {
        COSMORUN_ERROR_NULL(COSMORUN_ERR_NULL_POINTER, "readline context or history is NULL");
    }

    size_t history_len = std_vector_len(rl->history);
    if (index < 0 || (size_t)index >= history_len) {
        COSMORUN_ERROR_NULL(COSMORUN_ERR_INDEX_OUT_OF_BOUNDS, "history index out of bounds");
    }

    std_string_t *entry = (std_string_t*)std_vector_get(rl->history, index);
    return entry ? std_string_cstr(entry) : NULL;
}

size_t readline_history_size(readline_t *rl) {
    if (!rl || !rl->history) return 0;
    return std_vector_len(rl->history);
}

void readline_history_clear(readline_t *rl) {
    if (!rl || !rl->history) return;

    // Free all history entries
    for (size_t i = 0; i < std_vector_len(rl->history); i++) {
        std_string_t *entry = (std_string_t*)std_vector_get(rl->history, i);
        std_string_free(entry);
    }

    // Clear vector
    std_vector_clear(rl->history);
}

void readline_history_set_max_size(readline_t *rl, size_t max_size) {
    if (!rl) return;

    rl->history_max_size = max_size;

    // Remove oldest entries if we exceed the new limit
    if (max_size > 0 && rl->history) {
        while (std_vector_len(rl->history) > max_size) {
            std_string_t *oldest = (std_string_t*)std_vector_get(rl->history, 0);
            std_string_free(oldest);

            // Shift all entries down
            for (size_t i = 0; i < std_vector_len(rl->history) - 1; i++) {
                std_vector_set(rl->history, i, std_vector_get(rl->history, i + 1));
            }
            rl->history->len--;
        }
    }
}

/* ==================== History Persistence ==================== */

int readline_history_save(readline_t *rl, const char *filename) {
    if (!rl || !rl->history || !filename) {
        COSMORUN_ERROR_RETURN(COSMORUN_ERR_INVALID_ARG, "Invalid arguments to readline_history_save", -1);
    }

    FILE *file = fopen(filename, "w");
    if (!file) {
        COSMORUN_ERROR_RETURN(COSMORUN_ERR_FILE_OPEN_FAILED, "Failed to open history file for writing", -1);
    }

    // Write each history entry as a line
    size_t history_len = std_vector_len(rl->history);
    for (size_t i = 0; i < history_len; i++) {
        std_string_t *entry = (std_string_t*)std_vector_get(rl->history, i);
        if (entry) {
            fprintf(file, "%s\n", std_string_cstr(entry));
        }
    }

    fclose(file);
    return 0;
}

int readline_history_load(readline_t *rl, const char *filename) {
    if (!rl || !rl->history || !filename) {
        COSMORUN_ERROR_RETURN(COSMORUN_ERR_INVALID_ARG, "Invalid arguments to readline_history_load", -1);
    }

    FILE *file = fopen(filename, "r");
    if (!file) {
        COSMORUN_ERROR_RETURN(COSMORUN_ERR_FILE_OPEN_FAILED, "Failed to open history file for reading", -1);
    }

    int loaded = 0;
    char buffer[4096];

    while (fgets(buffer, sizeof(buffer), file)) {
        // Trim trailing whitespace
        trim_trailing_whitespace(buffer);

        // Skip empty lines
        if (is_empty_or_whitespace(buffer)) {
            continue;
        }

        // Add to history
        if (readline_history_add(rl, buffer) == 0) {
            loaded++;
        }
    }

    fclose(file);
    return loaded;
}

/* ==================== Event API ==================== */

void readline_set_emitter(readline_t *rl, event_emitter_t *emitter) {
    if (!rl) return;

    // Free old emitter if exists
    if (rl->emitter) {
        event_emitter_free(rl->emitter);
    }

    rl->emitter = emitter;
}

/* ==================== Test Mode API ==================== */

readline_t* readline_create_test(const char *prompt, const char **test_inputs, int test_input_count) {
    readline_t *rl = readline_create(prompt);
    if (!rl) {
        COSMORUN_ERROR_NULL(COSMORUN_ERR_OUT_OF_MEMORY, "Failed to create test readline context");
    }

    rl->test_mode = 1;
    rl->test_inputs = test_inputs;
    rl->test_input_index = 0;
    rl->test_input_count = test_input_count;

    return rl;
}
