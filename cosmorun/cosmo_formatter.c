/* cosmo_formatter.c - Simple code formatter implementation
 *
 * Line-by-line code formatting with basic rules:
 * - Indentation control (tabs/spaces)
 * - Line length wrapping
 * - Whitespace cleanup
 * - K&R brace style
 * - Operator spacing
 */

#include "cosmo_formatter.h"
#include "cosmo_libc.h"
#include <string.h>
#include <ctype.h>

/* Internal state for formatter */
typedef struct {
    char *output;
    size_t output_size;
    size_t output_capacity;
    int indent_level;
    int in_string;
    int in_char;
    int in_comment;
    int in_block_comment;
    int prev_was_backslash;
    const format_options_t *opts;
} formatter_state_t;

/* Helper: Initialize formatter state */
static formatter_state_t* formatter_state_init(const format_options_t *opts) {
    formatter_state_t *state = calloc(1, sizeof(formatter_state_t));
    if (!state) return NULL;

    state->output_capacity = 64 * 1024; /* Start with 64KB */
    state->output = malloc(state->output_capacity);
    if (!state->output) {
        free(state);
        return NULL;
    }

    state->output_size = 0;
    state->indent_level = 0;
    state->opts = opts;
    return state;
}

/* Helper: Free formatter state */
static void formatter_state_free(formatter_state_t *state) {
    if (state) {
        free(state->output);
        free(state);
    }
}

/* Helper: Append character to output buffer */
static int append_char(formatter_state_t *state, char c) {
    if (state->output_size + 1 >= state->output_capacity) {
        size_t new_capacity = state->output_capacity * 2;
        char *new_output = realloc(state->output, new_capacity);
        if (!new_output) return -1;
        state->output = new_output;
        state->output_capacity = new_capacity;
    }
    state->output[state->output_size++] = c;
    return 0;
}

/* Helper: Append string to output buffer */
static int append_string(formatter_state_t *state, const char *str) {
    while (*str) {
        if (append_char(state, *str++) < 0) return -1;
    }
    return 0;
}

/* Helper: Check if character is operator */
static int is_operator_char(char c) {
    return (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
            c == '=' || c == '<' || c == '>' || c == '!' || c == '&' ||
            c == '|' || c == '^' || c == '~');
}

/* Helper: Get operator length (handles multi-char operators like ++, ==, etc.) */
static int get_operator_length(const char *p) {
    if (!p || !*p) return 0;

    /* Two-character operators */
    if (p[0] && p[1]) {
        if ((p[0] == '+' && p[1] == '+') || /* ++ */
            (p[0] == '-' && p[1] == '-') || /* -- */
            (p[0] == '=' && p[1] == '=') || /* == */
            (p[0] == '!' && p[1] == '=') || /* != */
            (p[0] == '<' && p[1] == '=') || /* <= */
            (p[0] == '>' && p[1] == '=') || /* >= */
            (p[0] == '&' && p[1] == '&') || /* && */
            (p[0] == '|' && p[1] == '|') || /* || */
            (p[0] == '<' && p[1] == '<') || /* << */
            (p[0] == '>' && p[1] == '>') || /* >> */
            (p[0] == '+' && p[1] == '=') || /* += */
            (p[0] == '-' && p[1] == '=') || /* -= */
            (p[0] == '*' && p[1] == '=') || /* *= */
            (p[0] == '/' && p[1] == '=') || /* /= */
            (p[0] == '%' && p[1] == '=') || /* %= */
            (p[0] == '&' && p[1] == '=') || /* &= */
            (p[0] == '|' && p[1] == '=') || /* |= */
            (p[0] == '^' && p[1] == '=') || /* ^= */
            (p[0] == '-' && p[1] == '>'))   /* -> */
        {
            return 2;
        }
    }

    /* Single-character operator */
    if (is_operator_char(p[0])) {
        return 1;
    }

    return 0;
}

/* Helper: Remove trailing whitespace from current line */
static void remove_trailing_ws(formatter_state_t *state) {
    while (state->output_size > 0) {
        char c = state->output[state->output_size - 1];
        if (c == ' ' || c == '\t') {
            state->output_size--;
        } else {
            break;
        }
    }
}

/* Helper: Add indentation at start of line */
static int add_indentation(formatter_state_t *state) {
    for (int i = 0; i < state->indent_level; i++) {
        if (state->opts->use_tabs) {
            if (append_char(state, '\t') < 0) return -1;
        } else {
            for (int j = 0; j < state->opts->indent_size; j++) {
                if (append_char(state, ' ') < 0) return -1;
            }
        }
    }
    return 0;
}

/* Helper: Count leading whitespace */
static int count_leading_ws(const char *line) {
    int count = 0;
    while (*line == ' ' || *line == '\t') {
        line++;
        count++;
    }
    return count;
}

/* Helper: Trim string */
static char* trim_string(char *str) {
    while (*str && isspace(*str)) str++;
    if (*str == '\0') return str;

    char *end = str + strlen(str) - 1;
    while (end > str && isspace(*end)) end--;
    *(end + 1) = '\0';

    return str;
}

/* Helper: Check if line is preprocessor directive */
static int is_preprocessor(const char *line) {
    while (*line && isspace(*line)) line++;
    return *line == '#';
}

/* Helper: Check if line opens a block */
static int opens_block(const char *line) {
    const char *p = line + strlen(line) - 1;
    while (p >= line && isspace(*p)) p--;
    return (p >= line && *p == '{');
}

/* Helper: Check if line closes a block */
static int closes_block(const char *line) {
    while (*line && isspace(*line)) line++;
    return *line == '}';
}

/* Helper: Count braces in line (for indentation tracking) */
static void update_indent_level(formatter_state_t *state, const char *line) {
    for (const char *p = line; *p; p++) {
        /* Skip strings and comments */
        if (state->in_string || state->in_char ||
            state->in_comment || state->in_block_comment) {
            continue;
        }

        if (*p == '{') {
            state->indent_level++;
        } else if (*p == '}') {
            if (state->indent_level > 0) {
                state->indent_level--;
            }
        }
    }
}

/* Helper: Format a single line */
static int format_line(formatter_state_t *state, char *line) {
    if (!line || !*line) {
        /* Empty line - just add newline */
        return append_char(state, '\n');
    }

    /* Trim leading/trailing whitespace */
    char *trimmed = trim_string(line);
    if (!*trimmed) {
        return append_char(state, '\n');
    }

    /* Handle closing brace - decrease indent before line */
    int is_closing = closes_block(trimmed);
    if (is_closing && state->indent_level > 0) {
        state->indent_level--;
    }

    /* Preprocessor directives at column 0 */
    if (is_preprocessor(trimmed)) {
        if (append_string(state, trimmed) < 0) return -1;
        if (append_char(state, '\n') < 0) return -1;
        return 0;
    }

    /* Add indentation */
    if (add_indentation(state) < 0) return -1;

    /* Process line content with spacing rules */
    const char *p = trimmed;
    int prev_was_space = 0;
    int line_len = state->indent_level * (state->opts->use_tabs ? 1 : state->opts->indent_size);

    while (*p) {
        char c = *p;

        /* Track string/comment state */
        if (!state->in_comment && !state->in_block_comment) {
            if (c == '"' && !state->prev_was_backslash) {
                state->in_string = !state->in_string;
            } else if (c == '\'' && !state->prev_was_backslash) {
                state->in_char = !state->in_char;
            }
        }

        /* Track comment state */
        if (!state->in_string && !state->in_char) {
            if (c == '/' && *(p+1) == '/') {
                state->in_comment = 1;
            } else if (c == '/' && *(p+1) == '*') {
                state->in_block_comment = 1;
            } else if (state->in_block_comment && c == '*' && *(p+1) == '/') {
                state->in_block_comment = 0;
                if (append_char(state, c) < 0) return -1;
                p++;
                c = *p;
                line_len += 2;
            }
        }

        /* Apply spacing rules only outside strings/comments */
        if (!state->in_string && !state->in_char &&
            !state->in_comment && !state->in_block_comment) {

            /* Space around operators */
            int op_len = get_operator_length(p);
            if (state->opts->space_around_ops && op_len > 0) {
                /* Special handling for ++ and -- (no spaces) */
                int is_increment = (op_len == 2 && (p[0] == '+' || p[0] == '-') && p[1] == p[0]);
                /* Special handling for -> (no spaces) */
                int is_arrow = (op_len == 2 && p[0] == '-' && p[1] == '>');

                if (!is_increment && !is_arrow) {
                    /* Check if we need space before */
                    if (state->output_size > 0 &&
                        !isspace(state->output[state->output_size - 1]) &&
                        state->output[state->output_size - 1] != '(') {
                        if (append_char(state, ' ') < 0) return -1;
                        line_len++;
                    }
                }

                /* Output the operator */
                for (int i = 0; i < op_len; i++) {
                    if (append_char(state, p[i]) < 0) return -1;
                    line_len++;
                }

                if (!is_increment && !is_arrow) {
                    /* Add space after if next char is not operator or space */
                    if (*(p + op_len) && !is_operator_char(*(p + op_len)) &&
                        !isspace(*(p + op_len)) && *(p + op_len) != ';' &&
                        *(p + op_len) != ')' && *(p + op_len) != ']') {
                        if (append_char(state, ' ') < 0) return -1;
                        line_len++;
                    }
                }

                p += op_len;
                prev_was_space = 0;
                continue;
            }

            /* Space after comma */
            if (state->opts->space_after_comma && c == ',') {
                if (append_char(state, c) < 0) return -1;
                line_len++;
                if (*(p+1) && !isspace(*(p+1))) {
                    if (append_char(state, ' ') < 0) return -1;
                    line_len++;
                }
                p++;
                prev_was_space = 0;
                continue;
            }

            /* Collapse multiple spaces */
            if (isspace(c)) {
                if (!prev_was_space) {
                    if (append_char(state, ' ') < 0) return -1;
                    line_len++;
                    prev_was_space = 1;
                }
                p++;
                continue;
            }
        }

        /* Normal character */
        if (append_char(state, c) < 0) return -1;
        line_len++;
        prev_was_space = 0;

        state->prev_was_backslash = (c == '\\');
        p++;
    }

    /* Remove trailing whitespace if enabled */
    if (state->opts->remove_trailing_ws) {
        remove_trailing_ws(state);
    }

    /* Add newline */
    if (append_char(state, '\n') < 0) return -1;

    /* Handle opening brace - increase indent after line */
    if (opens_block(trimmed)) {
        state->indent_level++;
    }

    /* Reset line comment state */
    state->in_comment = 0;

    return 0;
}

/* Initialize default formatting options */
void format_options_init_default(format_options_t *opts) {
    opts->indent_size = 4;
    opts->use_tabs = 0;
    opts->max_line_length = 100;
    opts->brace_style = BRACE_STYLE_KR;
    opts->space_around_ops = 1;
    opts->space_after_comma = 1;
    opts->remove_trailing_ws = 1;
    opts->normalize_blank_lines = 1;
}

/* Load formatting options from config file */
int format_options_load_from_file(format_options_t *opts, const char *config_file) {
    FILE *f = fopen(config_file, "r");
    if (!f) return FORMAT_ERROR_IO;

    format_options_init_default(opts);

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *trimmed = trim_string(line);
        if (!*trimmed || *trimmed == '#') continue;

        char key[64], value[64];
        if (sscanf(trimmed, "%63[^=]=%63s", key, value) == 2) {
            char *key_trim = trim_string(key);
            char *val_trim = trim_string(value);

            if (strcmp(key_trim, "indent_size") == 0) {
                opts->indent_size = atoi(val_trim);
            } else if (strcmp(key_trim, "use_tabs") == 0) {
                opts->use_tabs = atoi(val_trim);
            } else if (strcmp(key_trim, "max_line_length") == 0) {
                opts->max_line_length = atoi(val_trim);
            } else if (strcmp(key_trim, "brace_style") == 0) {
                if (strcmp(val_trim, "kr") == 0) opts->brace_style = BRACE_STYLE_KR;
                else if (strcmp(val_trim, "allman") == 0) opts->brace_style = BRACE_STYLE_ALLMAN;
                else if (strcmp(val_trim, "gnu") == 0) opts->brace_style = BRACE_STYLE_GNU;
            }
        }
    }

    fclose(f);
    return FORMAT_SUCCESS;
}

/* Format a string of C code */
format_result_t format_string(const char *input, const format_options_t *opts) {
    format_result_t result = {0};

    if (!input || !opts) {
        result.error_code = FORMAT_ERROR_INVALID;
        snprintf(result.error_msg, sizeof(result.error_msg), "Invalid input parameters");
        return result;
    }

    formatter_state_t *state = formatter_state_init(opts);
    if (!state) {
        result.error_code = FORMAT_ERROR_MEMORY;
        snprintf(result.error_msg, sizeof(result.error_msg), "Memory allocation failed");
        return result;
    }

    /* Process input line by line */
    char *input_copy = strdup(input);
    if (!input_copy) {
        formatter_state_free(state);
        result.error_code = FORMAT_ERROR_MEMORY;
        snprintf(result.error_msg, sizeof(result.error_msg), "Memory allocation failed");
        return result;
    }

    char *line = strtok(input_copy, "\n");
    int prev_was_blank = 0;

    while (line) {
        char *trimmed = trim_string(line);
        int is_blank = (*trimmed == '\0');

        /* Normalize blank lines */
        if (opts->normalize_blank_lines && is_blank) {
            if (!prev_was_blank) {
                if (format_line(state, "") < 0) {
                    free(input_copy);
                    formatter_state_free(state);
                    result.error_code = FORMAT_ERROR_MEMORY;
                    snprintf(result.error_msg, sizeof(result.error_msg), "Formatting failed");
                    return result;
                }
                prev_was_blank = 1;
            }
        } else {
            if (format_line(state, line) < 0) {
                free(input_copy);
                formatter_state_free(state);
                result.error_code = FORMAT_ERROR_MEMORY;
                snprintf(result.error_msg, sizeof(result.error_msg), "Formatting failed");
                return result;
            }
            prev_was_blank = is_blank;
        }

        line = strtok(NULL, "\n");
    }

    free(input_copy);

    /* Null-terminate output */
    if (append_char(state, '\0') < 0) {
        formatter_state_free(state);
        result.error_code = FORMAT_ERROR_MEMORY;
        snprintf(result.error_msg, sizeof(result.error_msg), "Formatting failed");
        return result;
    }

    result.content = state->output;
    result.content_size = state->output_size - 1; /* Exclude null terminator */
    result.error_code = FORMAT_SUCCESS;

    /* Don't free state->output, it's now owned by result */
    free(state);

    return result;
}

/* Format a file */
format_result_t format_file(const char *input_file, const format_options_t *opts) {
    format_result_t result = {0};

    FILE *f = fopen(input_file, "rb");
    if (!f) {
        result.error_code = FORMAT_ERROR_IO;
        snprintf(result.error_msg, sizeof(result.error_msg), "Cannot open file: %s", input_file);
        return result;
    }

    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    /* Read file content */
    char *content = malloc(file_size + 1);
    if (!content) {
        fclose(f);
        result.error_code = FORMAT_ERROR_MEMORY;
        snprintf(result.error_msg, sizeof(result.error_msg), "Memory allocation failed");
        return result;
    }

    size_t bytes_read = fread(content, 1, file_size, f);
    fclose(f);
    content[bytes_read] = '\0';

    /* Format content */
    result = format_string(content, opts);
    free(content);

    return result;
}

/* Write formatted result to file */
int write_formatted_file(const format_result_t *result, const char *output_file) {
    if (!result || !output_file || result->error_code != FORMAT_SUCCESS) {
        return FORMAT_ERROR_INVALID;
    }

    FILE *f = fopen(output_file, "wb");
    if (!f) {
        return FORMAT_ERROR_IO;
    }

    size_t written = fwrite(result->content, 1, result->content_size, f);
    fclose(f);

    if (written != result->content_size) {
        return FORMAT_ERROR_IO;
    }

    return FORMAT_SUCCESS;
}

/* Free format result resources */
void free_format_result(format_result_t *result) {
    if (result && result->content) {
        free(result->content);
        result->content = NULL;
        result->content_size = 0;
    }
}
