/*
 * mod_error.h - Unified error handling system for cosmorun
 *
 * Provides standardized error codes and reporting mechanism
 * Thread-safe error tracking with static buffers
 */

#ifndef MOD_ERROR_H
#define MOD_ERROR_H

/* ==================== Error Codes ==================== */

typedef enum {
    COSMORUN_OK = 0,

    /* General errors (-1 to -10) */
    COSMORUN_ERR_NULL_POINTER = -1,
    COSMORUN_ERR_INVALID_ARG = -2,
    COSMORUN_ERR_OUT_OF_MEMORY = -3,
    COSMORUN_ERR_NOT_IMPLEMENTED = -4,
    COSMORUN_ERR_UNSUPPORTED = -5,

    /* File/IO errors (-11 to -20) */
    COSMORUN_ERR_FILE_NOT_FOUND = -11,
    COSMORUN_ERR_FILE_OPEN_FAILED = -12,
    COSMORUN_ERR_FILE_READ_FAILED = -13,
    COSMORUN_ERR_FILE_WRITE_FAILED = -14,
    COSMORUN_ERR_IO_ERROR = -15,

    /* Module/symbol errors (-21 to -30) */
    COSMORUN_ERR_MODULE_LOAD_FAILED = -21,
    COSMORUN_ERR_SYMBOL_NOT_FOUND = -22,
    COSMORUN_ERR_INIT_FAILED = -23,
    COSMORUN_ERR_MODULE_NOT_LOADED = -24,

    /* Network errors (-31 to -40) */
    COSMORUN_ERR_NETWORK = -31,
    COSMORUN_ERR_CONNECTION_FAILED = -32,
    COSMORUN_ERR_TIMEOUT = -33,
    COSMORUN_ERR_DNS_FAILED = -34,
    COSMORUN_ERR_SOCKET_ERROR = -35,

    /* Parsing/format errors (-41 to -50) */
    COSMORUN_ERR_PARSE_FAILED = -41,
    COSMORUN_ERR_INVALID_FORMAT = -42,
    COSMORUN_ERR_SYNTAX_ERROR = -43,
    COSMORUN_ERR_ENCODING_ERROR = -44,

    /* Runtime errors (-51 to -60) */
    COSMORUN_ERR_BUFFER_OVERFLOW = -51,
    COSMORUN_ERR_BUFFER_UNDERFLOW = -52,
    COSMORUN_ERR_INDEX_OUT_OF_BOUNDS = -53,
    COSMORUN_ERR_ASSERTION_FAILED = -54,
} cosmorun_error_t;

/* ==================== Error Reporting API ==================== */

/* Set the last error with a simple message */
void cosmorun_set_error(cosmorun_error_t code, const char *msg);

/* Set the last error with a formatted message */
void cosmorun_set_error_fmt(cosmorun_error_t code, const char *fmt, ...);

/* Get the last error message */
const char* cosmorun_get_error_msg(void);

/* Get the last error code */
cosmorun_error_t cosmorun_get_last_error(void);

/* Clear the last error */
void cosmorun_clear_error(void);

/* Get human-readable error name */
const char* cosmorun_error_name(cosmorun_error_t code);

/* Get human-readable error description */
const char* cosmorun_error_desc(cosmorun_error_t code);

/* ==================== Convenience Macros ==================== */

/* Set error with simple message */
#define COSMORUN_ERROR(code, msg) cosmorun_set_error(code, msg)

/* Set error with formatted message */
#define COSMORUN_ERROR_FMT(code, fmt, ...) \
    cosmorun_set_error_fmt(code, fmt, ##__VA_ARGS__)

/* Set error and return value */
#define COSMORUN_ERROR_RETURN(code, msg, ret) \
    do { cosmorun_set_error(code, msg); return (ret); } while(0)

/* Set error and return NULL */
#define COSMORUN_ERROR_NULL(code, msg) \
    COSMORUN_ERROR_RETURN(code, msg, NULL)

/* Check pointer and set NULL_POINTER error if null */
#define COSMORUN_CHECK_NULL(ptr, msg) \
    do { if (!(ptr)) { COSMORUN_ERROR_RETURN(COSMORUN_ERR_NULL_POINTER, msg, -1); } } while(0)

#endif /* MOD_ERROR_H */
