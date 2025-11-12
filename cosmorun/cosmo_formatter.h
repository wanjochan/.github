/* cosmo_formatter.h - Code formatting API
 *
 * Simple code formatter for consistent style across codebase.
 * Supports basic formatting rules: indentation, line length,
 * whitespace cleanup, brace style, and operator spacing.
 */

#ifndef COSMO_FORMATTER_H
#define COSMO_FORMATTER_H

#include <stddef.h>

/* Brace style options */
typedef enum {
    BRACE_STYLE_KR,        /* K&R: opening brace on same line */
    BRACE_STYLE_ALLMAN,    /* Allman: opening brace on new line */
    BRACE_STYLE_GNU        /* GNU: brace indented by half indent */
} brace_style_t;

/* Formatter options */
typedef struct {
    int indent_size;           /* Number of spaces per indent level (default: 4) */
    int use_tabs;              /* Use tabs instead of spaces (default: 0) */
    int max_line_length;       /* Maximum line length (default: 100) */
    brace_style_t brace_style; /* Brace style (default: K&R) */
    int space_around_ops;      /* Add spaces around operators (default: 1) */
    int space_after_comma;     /* Add space after commas (default: 1) */
    int remove_trailing_ws;    /* Remove trailing whitespace (default: 1) */
    int normalize_blank_lines; /* Normalize blank lines (default: 1) */
} format_options_t;

/* Format result structure */
typedef struct {
    char *content;             /* Formatted code content */
    size_t content_size;       /* Size of formatted content */
    int error_code;            /* Error code (0 = success) */
    char error_msg[256];       /* Error message if error_code != 0 */
} format_result_t;

/* Initialize default formatting options */
void format_options_init_default(format_options_t *opts);

/* Load formatting options from .cosmoformat file */
int format_options_load_from_file(format_options_t *opts, const char *config_file);

/* Format a string of C code */
format_result_t format_string(const char *input, const format_options_t *opts);

/* Format a file */
format_result_t format_file(const char *input_file, const format_options_t *opts);

/* Write formatted result to file */
int write_formatted_file(const format_result_t *result, const char *output_file);

/* Free format result resources */
void free_format_result(format_result_t *result);

/* Format error codes */
#define FORMAT_SUCCESS           0
#define FORMAT_ERROR_MEMORY     -1
#define FORMAT_ERROR_IO         -2
#define FORMAT_ERROR_INVALID    -3

#endif /* COSMO_FORMATTER_H */
