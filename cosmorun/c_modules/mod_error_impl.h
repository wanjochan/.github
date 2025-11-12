/*
 * mod_error_impl.h - Error handling implementation (header-only)
 *
 * Include this file in ONE .c file per module to get the implementation
 */

#ifndef MOD_ERROR_IMPL_H
#define MOD_ERROR_IMPL_H

#include "mod_error.h"

/* ==================== Error State ==================== */

/* Global error state (static for each compilation unit) */
static cosmorun_error_t g_last_error = COSMORUN_OK;
static char g_error_msg[256] = {0};

/* ==================== Error Reporting Functions ==================== */

void cosmorun_set_error(cosmorun_error_t code, const char *msg) {
    g_last_error = code;
    if (msg) {
        /* Simple string copy without snprintf to avoid stdio.h */
        int i = 0;
        while (i < 255 && msg[i]) {
            g_error_msg[i] = msg[i];
            i++;
        }
        g_error_msg[i] = '\0';
    } else {
        g_error_msg[0] = '\0';
    }
}

void cosmorun_set_error_fmt(cosmorun_error_t code, const char *fmt, ...) {
    /* Simplified version - just copy the format string for now */
    /* Full implementation would require varargs support */
    g_last_error = code;
    if (fmt) {
        int i = 0;
        while (i < 255 && fmt[i]) {
            g_error_msg[i] = fmt[i];
            i++;
        }
        g_error_msg[i] = '\0';
    } else {
        g_error_msg[0] = '\0';
    }
}

const char* cosmorun_get_error_msg(void) {
    return g_error_msg;
}

cosmorun_error_t cosmorun_get_last_error(void) {
    return g_last_error;
}

void cosmorun_clear_error(void) {
    g_last_error = COSMORUN_OK;
    g_error_msg[0] = '\0';
}

/* ==================== Error Code Information ==================== */

const char* cosmorun_error_name(cosmorun_error_t code) {
    switch (code) {
        case COSMORUN_OK: return "OK";

        /* General errors */
        case COSMORUN_ERR_NULL_POINTER: return "NULL_POINTER";
        case COSMORUN_ERR_INVALID_ARG: return "INVALID_ARG";
        case COSMORUN_ERR_OUT_OF_MEMORY: return "OUT_OF_MEMORY";
        case COSMORUN_ERR_NOT_IMPLEMENTED: return "NOT_IMPLEMENTED";
        case COSMORUN_ERR_UNSUPPORTED: return "UNSUPPORTED";

        /* File/IO errors */
        case COSMORUN_ERR_FILE_NOT_FOUND: return "FILE_NOT_FOUND";
        case COSMORUN_ERR_FILE_OPEN_FAILED: return "FILE_OPEN_FAILED";
        case COSMORUN_ERR_FILE_READ_FAILED: return "FILE_READ_FAILED";
        case COSMORUN_ERR_FILE_WRITE_FAILED: return "FILE_WRITE_FAILED";
        case COSMORUN_ERR_IO_ERROR: return "IO_ERROR";

        /* Module/symbol errors */
        case COSMORUN_ERR_MODULE_LOAD_FAILED: return "MODULE_LOAD_FAILED";
        case COSMORUN_ERR_SYMBOL_NOT_FOUND: return "SYMBOL_NOT_FOUND";
        case COSMORUN_ERR_INIT_FAILED: return "INIT_FAILED";
        case COSMORUN_ERR_MODULE_NOT_LOADED: return "MODULE_NOT_LOADED";

        /* Network errors */
        case COSMORUN_ERR_NETWORK: return "NETWORK";
        case COSMORUN_ERR_CONNECTION_FAILED: return "CONNECTION_FAILED";
        case COSMORUN_ERR_TIMEOUT: return "TIMEOUT";
        case COSMORUN_ERR_DNS_FAILED: return "DNS_FAILED";
        case COSMORUN_ERR_SOCKET_ERROR: return "SOCKET_ERROR";

        /* Parsing/format errors */
        case COSMORUN_ERR_PARSE_FAILED: return "PARSE_FAILED";
        case COSMORUN_ERR_INVALID_FORMAT: return "INVALID_FORMAT";
        case COSMORUN_ERR_SYNTAX_ERROR: return "SYNTAX_ERROR";
        case COSMORUN_ERR_ENCODING_ERROR: return "ENCODING_ERROR";

        /* Runtime errors */
        case COSMORUN_ERR_BUFFER_OVERFLOW: return "BUFFER_OVERFLOW";
        case COSMORUN_ERR_BUFFER_UNDERFLOW: return "BUFFER_UNDERFLOW";
        case COSMORUN_ERR_INDEX_OUT_OF_BOUNDS: return "INDEX_OUT_OF_BOUNDS";
        case COSMORUN_ERR_ASSERTION_FAILED: return "ASSERTION_FAILED";

        default: return "UNKNOWN_ERROR";
    }
}

const char* cosmorun_error_desc(cosmorun_error_t code) {
    switch (code) {
        case COSMORUN_OK: return "Success";

        /* General errors */
        case COSMORUN_ERR_NULL_POINTER: return "Null pointer dereference";
        case COSMORUN_ERR_INVALID_ARG: return "Invalid argument";
        case COSMORUN_ERR_OUT_OF_MEMORY: return "Out of memory";
        case COSMORUN_ERR_NOT_IMPLEMENTED: return "Feature not implemented";
        case COSMORUN_ERR_UNSUPPORTED: return "Operation not supported";

        /* File/IO errors */
        case COSMORUN_ERR_FILE_NOT_FOUND: return "File not found";
        case COSMORUN_ERR_FILE_OPEN_FAILED: return "Failed to open file";
        case COSMORUN_ERR_FILE_READ_FAILED: return "Failed to read file";
        case COSMORUN_ERR_FILE_WRITE_FAILED: return "Failed to write file";
        case COSMORUN_ERR_IO_ERROR: return "I/O error";

        /* Module/symbol errors */
        case COSMORUN_ERR_MODULE_LOAD_FAILED: return "Failed to load module";
        case COSMORUN_ERR_SYMBOL_NOT_FOUND: return "Symbol not found";
        case COSMORUN_ERR_INIT_FAILED: return "Initialization failed";
        case COSMORUN_ERR_MODULE_NOT_LOADED: return "Module not loaded";

        /* Network errors */
        case COSMORUN_ERR_NETWORK: return "Network error";
        case COSMORUN_ERR_CONNECTION_FAILED: return "Connection failed";
        case COSMORUN_ERR_TIMEOUT: return "Operation timed out";
        case COSMORUN_ERR_DNS_FAILED: return "DNS resolution failed";
        case COSMORUN_ERR_SOCKET_ERROR: return "Socket error";

        /* Parsing/format errors */
        case COSMORUN_ERR_PARSE_FAILED: return "Parse error";
        case COSMORUN_ERR_INVALID_FORMAT: return "Invalid format";
        case COSMORUN_ERR_SYNTAX_ERROR: return "Syntax error";
        case COSMORUN_ERR_ENCODING_ERROR: return "Encoding error";

        /* Runtime errors */
        case COSMORUN_ERR_BUFFER_OVERFLOW: return "Buffer overflow";
        case COSMORUN_ERR_BUFFER_UNDERFLOW: return "Buffer underflow";
        case COSMORUN_ERR_INDEX_OUT_OF_BOUNDS: return "Index out of bounds";
        case COSMORUN_ERR_ASSERTION_FAILED: return "Assertion failed";

        default: return "Unknown error";
    }
}

#endif /* MOD_ERROR_IMPL_H */
