/*
 * cosmo_errors.h - Enhanced error handling system for CosmoRun
 *
 * Provides:
 * - Structured error codes with categories
 * - Error severity levels
 * - Error context and stack tracing
 * - Multi-language error messages
 * - Auto-recovery hints
 */

#ifndef COSMO_ERRORS_H
#define COSMO_ERRORS_H

#include <stddef.h>
#include <stdint.h>

/* ==================== Error Severity Levels ==================== */

typedef enum {
    COSMO_SEV_INFO = 0,      /* Informational message */
    COSMO_SEV_WARNING = 1,   /* Warning - operation continues */
    COSMO_SEV_ERROR = 2,     /* Error - operation failed */
    COSMO_SEV_FATAL = 3      /* Fatal - system cannot continue */
} cosmo_severity_t;

/* ==================== Enhanced Error Codes ==================== */

typedef enum {
    /* Success */
    COSMO_ERR_OK = 0,

    /* General errors (1-99) */
    COSMO_ERR_GENERAL_UNKNOWN = -1,
    COSMO_ERR_GENERAL_NULL_POINTER = -2,
    COSMO_ERR_GENERAL_INVALID_ARG = -3,
    COSMO_ERR_GENERAL_OUT_OF_MEMORY = -4,
    COSMO_ERR_GENERAL_NOT_IMPLEMENTED = -5,
    COSMO_ERR_GENERAL_UNSUPPORTED = -6,
    COSMO_ERR_GENERAL_PERMISSION = -7,
    COSMO_ERR_GENERAL_STATE_INVALID = -8,
    COSMO_ERR_GENERAL_TIMEOUT = -9,
    COSMO_ERR_GENERAL_BUSY = -10,

    /* File/IO errors (100-199) */
    COSMO_ERR_IO_FILE_NOT_FOUND = -100,
    COSMO_ERR_IO_FILE_OPEN_FAILED = -101,
    COSMO_ERR_IO_FILE_READ_FAILED = -102,
    COSMO_ERR_IO_FILE_WRITE_FAILED = -103,
    COSMO_ERR_IO_FILE_CLOSE_FAILED = -104,
    COSMO_ERR_IO_FILE_SEEK_FAILED = -105,
    COSMO_ERR_IO_FILE_STAT_FAILED = -106,
    COSMO_ERR_IO_DIR_NOT_FOUND = -107,
    COSMO_ERR_IO_DIR_CREATE_FAILED = -108,
    COSMO_ERR_IO_PATH_TOO_LONG = -109,
    COSMO_ERR_IO_DISK_FULL = -110,
    COSMO_ERR_IO_PERMISSION_DENIED = -111,

    /* Compilation errors (200-299) */
    COSMO_ERR_COMPILE_SYNTAX_ERROR = -200,
    COSMO_ERR_COMPILE_TYPE_MISMATCH = -201,
    COSMO_ERR_COMPILE_UNDECLARED_VAR = -202,
    COSMO_ERR_COMPILE_REDEFINED_SYMBOL = -203,
    COSMO_ERR_COMPILE_MISSING_SEMICOLON = -204,
    COSMO_ERR_COMPILE_UNCLOSED_BRACKET = -205,
    COSMO_ERR_COMPILE_UNCLOSED_STRING = -206,
    COSMO_ERR_COMPILE_INVALID_DIRECTIVE = -207,
    COSMO_ERR_COMPILE_MACRO_EXPANSION = -208,
    COSMO_ERR_COMPILE_INCOMPATIBLE_TYPE = -209,
    COSMO_ERR_COMPILE_TOO_MANY_ERRORS = -210,

    /* Linking errors (300-399) */
    COSMO_ERR_LINK_UNDEFINED_SYMBOL = -300,
    COSMO_ERR_LINK_DUPLICATE_SYMBOL = -301,
    COSMO_ERR_LINK_LIBRARY_NOT_FOUND = -302,
    COSMO_ERR_LINK_CIRCULAR_DEPENDENCY = -303,
    COSMO_ERR_LINK_RELOCATION_FAILED = -304,
    COSMO_ERR_LINK_SYMBOL_RESOLUTION = -305,
    COSMO_ERR_LINK_VERSION_MISMATCH = -306,

    /* Module errors (400-499) */
    COSMO_ERR_MODULE_LOAD_FAILED = -400,
    COSMO_ERR_MODULE_INIT_FAILED = -401,
    COSMO_ERR_MODULE_NOT_FOUND = -402,
    COSMO_ERR_MODULE_SYMBOL_NOT_FOUND = -403,
    COSMO_ERR_MODULE_ALREADY_LOADED = -404,
    COSMO_ERR_MODULE_INCOMPATIBLE = -405,
    COSMO_ERR_MODULE_DEPENDENCY_MISSING = -406,
    COSMO_ERR_MODULE_UNLOAD_FAILED = -407,

    /* Runtime errors (500-599) */
    COSMO_ERR_RUNTIME_BUFFER_OVERFLOW = -500,
    COSMO_ERR_RUNTIME_BUFFER_UNDERFLOW = -501,
    COSMO_ERR_RUNTIME_INDEX_OUT_OF_BOUNDS = -502,
    COSMO_ERR_RUNTIME_ASSERTION_FAILED = -503,
    COSMO_ERR_RUNTIME_DIVISION_BY_ZERO = -504,
    COSMO_ERR_RUNTIME_NULL_DEREFERENCE = -505,
    COSMO_ERR_RUNTIME_STACK_OVERFLOW = -506,
    COSMO_ERR_RUNTIME_SEGFAULT = -507,
    COSMO_ERR_RUNTIME_SIGNAL_CAUGHT = -508,

    /* Network errors (600-699) */
    COSMO_ERR_NET_CONNECTION_FAILED = -600,
    COSMO_ERR_NET_DNS_FAILED = -601,
    COSMO_ERR_NET_TIMEOUT = -602,
    COSMO_ERR_NET_SOCKET_ERROR = -603,
    COSMO_ERR_NET_BIND_FAILED = -604,
    COSMO_ERR_NET_LISTEN_FAILED = -605,
    COSMO_ERR_NET_ACCEPT_FAILED = -606,
    COSMO_ERR_NET_SEND_FAILED = -607,
    COSMO_ERR_NET_RECV_FAILED = -608,

    /* Parsing errors (700-799) */
    COSMO_ERR_PARSE_INVALID_FORMAT = -700,
    COSMO_ERR_PARSE_UNEXPECTED_TOKEN = -701,
    COSMO_ERR_PARSE_UNEXPECTED_EOF = -702,
    COSMO_ERR_PARSE_INVALID_NUMBER = -703,
    COSMO_ERR_PARSE_INVALID_STRING = -704,
    COSMO_ERR_PARSE_ENCODING_ERROR = -705,

    /* TCC specific errors (800-899) */
    COSMO_ERR_TCC_STATE_NULL = -800,
    COSMO_ERR_TCC_COMPILE_FAILED = -801,
    COSMO_ERR_TCC_LINK_FAILED = -802,
    COSMO_ERR_TCC_RELOC_FAILED = -803,
    COSMO_ERR_TCC_INCLUDE_NOT_FOUND = -804,
    COSMO_ERR_TCC_LIBRARY_NOT_FOUND = -805,
    COSMO_ERR_TCC_OUTPUT_FAILED = -806,

} cosmo_error_code_t;

/* ==================== Error Location Information ==================== */

typedef struct {
    const char *file;           /* Source file path */
    int line;                   /* Line number (1-based) */
    int column;                 /* Column number (1-based) */
    const char *function;       /* Function name */
} cosmo_error_location_t;

/* ==================== Error Context ==================== */

#define COSMO_ERROR_MSG_SIZE 512
#define COSMO_ERROR_HINT_SIZE 256
#define COSMO_ERROR_CONTEXT_LINES 3

typedef struct {
    cosmo_error_code_t code;                    /* Error code */
    cosmo_severity_t severity;                  /* Severity level */
    char message[COSMO_ERROR_MSG_SIZE];         /* Error message */
    char hint[COSMO_ERROR_HINT_SIZE];           /* Fix suggestion */
    cosmo_error_location_t location;            /* Where error occurred */
    char context_lines[COSMO_ERROR_CONTEXT_LINES][256]; /* Source context */
    int context_count;                          /* Number of context lines */
    uint64_t timestamp;                         /* When error occurred */
    int recoverable;                            /* Can auto-recover? */
} cosmo_error_context_t;

/* ==================== Error Stack for Tracing ==================== */

#define COSMO_ERROR_STACK_MAX 32

typedef struct {
    cosmo_error_context_t errors[COSMO_ERROR_STACK_MAX];
    int count;
    int max_errors;      /* Stop after this many errors */
    int suppress_warnings;
} cosmo_error_stack_t;

/* ==================== Error Handling API ==================== */

/* Initialize error stack */
void cosmo_error_stack_init(cosmo_error_stack_t *stack);

/* Push error to stack */
int cosmo_error_push(cosmo_error_stack_t *stack,
                     cosmo_error_code_t code,
                     cosmo_severity_t severity,
                     const char *file, int line, int column,
                     const char *function,
                     const char *message);

/* Push formatted error */
int cosmo_error_pushf(cosmo_error_stack_t *stack,
                      cosmo_error_code_t code,
                      cosmo_severity_t severity,
                      const char *file, int line, int column,
                      const char *function,
                      const char *fmt, ...);

/* Get last error */
const cosmo_error_context_t* cosmo_error_get_last(const cosmo_error_stack_t *stack);

/* Get error at index */
const cosmo_error_context_t* cosmo_error_get_at(const cosmo_error_stack_t *stack, int index);

/* Clear error stack */
void cosmo_error_stack_clear(cosmo_error_stack_t *stack);

/* Print error with full context */
void cosmo_error_print(const cosmo_error_context_t *error);

/* Print all errors in stack */
void cosmo_error_stack_print(const cosmo_error_stack_t *stack);

/* Get error category name */
const char* cosmo_error_category(cosmo_error_code_t code);

/* Get error name (e.g., "FILE_NOT_FOUND") */
const char* cosmo_error_name(cosmo_error_code_t code);

/* Get error description (English) */
const char* cosmo_error_desc_en(cosmo_error_code_t code);

/* Get error description (Chinese) */
const char* cosmo_error_desc_zh(cosmo_error_code_t code);

/* Get severity name */
const char* cosmo_severity_name(cosmo_severity_t severity);

/* Get auto-recovery hint */
const char* cosmo_error_recovery_hint(cosmo_error_code_t code);

/* Check if error is recoverable */
int cosmo_error_is_recoverable(cosmo_error_code_t code);

/* ==================== Convenience Macros ==================== */

/* Report error with automatic location */
#define COSMO_ERROR(stack, code, severity, msg) \
    cosmo_error_push(stack, code, severity, __FILE__, __LINE__, 0, __func__, msg)

#define COSMO_ERROR_F(stack, code, severity, fmt, ...) \
    cosmo_error_pushf(stack, code, severity, __FILE__, __LINE__, 0, __func__, fmt, __VA_ARGS__)

/* Report with specific location */
#define COSMO_ERROR_AT(stack, code, severity, file, line, col, msg) \
    cosmo_error_push(stack, code, severity, file, line, col, __func__, msg)

/* Quick severity shortcuts */
#define COSMO_INFO(stack, code, msg) \
    COSMO_ERROR(stack, code, COSMO_SEV_INFO, msg)

#define COSMO_WARN(stack, code, msg) \
    COSMO_ERROR(stack, code, COSMO_SEV_WARNING, msg)

#define COSMO_ERR(stack, code, msg) \
    COSMO_ERROR(stack, code, COSMO_SEV_ERROR, msg)

#define COSMO_FATAL(stack, code, msg) \
    COSMO_ERROR(stack, code, COSMO_SEV_FATAL, msg)

#endif /* COSMO_ERRORS_H */
