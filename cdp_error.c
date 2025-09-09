/**
 * CDP Error Handling Module
 * Unified error management system
 */

#include "cdp_internal.h"
#include <stdarg.h>
#include <errno.h>

/* Error stack for better debugging */
#define ERROR_STACK_SIZE 10

typedef struct {
    CDPError code;
    char message[256];
    char context[128];
    char file[64];
    int line;
    time_t timestamp;
} CDPErrorEntry;

static struct {
    CDPErrorEntry stack[ERROR_STACK_SIZE];
    int top;
    int total_errors;
} g_error_state = { .top = -1, .total_errors = 0 };

/* Error code to string mapping */
static const char* error_strings[] = {
    [CDP_SUCCESS] = "Success",
    [CDP_ERR_CONNECTION_FAILED] = "Connection failed",
    [CDP_ERR_WEBSOCKET_FAILED] = "WebSocket error",
    [CDP_ERR_CHROME_NOT_FOUND] = "Chrome not found",
    [CDP_ERR_TIMEOUT] = "Operation timed out",
    [CDP_ERR_INVALID_RESPONSE] = "Invalid response",
    [CDP_ERR_COMMAND_FAILED] = "Command failed",
    [CDP_ERR_MEMORY] = "Memory allocation failed",
    [CDP_ERR_INVALID_ARGS] = "Invalid arguments"
};

/* Push error to stack */
int cdp_error_push_ex(CDPError code, const char *file, int line, const char *fmt, ...) {
    if (g_error_state.top >= ERROR_STACK_SIZE - 1) {
        // Stack full, remove oldest
        for (int i = 0; i < ERROR_STACK_SIZE - 1; i++) {
            g_error_state.stack[i] = g_error_state.stack[i + 1];
        }
        g_error_state.top = ERROR_STACK_SIZE - 2;
    }
    
    g_error_state.top++;
    CDPErrorEntry *entry = &g_error_state.stack[g_error_state.top];
    
    // Set basic info
    entry->code = code;
    entry->timestamp = time(NULL);
    str_copy_safe(entry->file, file, sizeof(entry->file));
    entry->line = line;
    
    // Format message
    if (fmt) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(entry->message, sizeof(entry->message), fmt, args);
        va_end(args);
    } else {
        str_copy_safe(entry->message, error_strings[code], sizeof(entry->message));
    }
    
    // Add system error if relevant
    if (errno != 0 && (code == CDP_ERR_CONNECTION_FAILED || 
                       code == CDP_ERR_WEBSOCKET_FAILED)) {
        snprintf(entry->context, sizeof(entry->context), "System error: %s", strerror(errno));
    } else {
        entry->context[0] = '\0';
    }
    
    g_error_state.total_errors++;
    
    // Log if in debug mode
    extern int debug_mode;
    if (debug_mode) {
        fprintf(stderr, "[ERROR] %s:%d: %s - %s\n", 
                file, line, error_strings[code], entry->message);
        if (entry->context[0]) {
            fprintf(stderr, "        Context: %s\n", entry->context);
        }
    }
    
    return code;
}

/* Get last error code */
CDPError cdp_error_last_code(void) {
    if (g_error_state.top >= 0) {
        return g_error_state.stack[g_error_state.top].code;
    }
    return CDP_SUCCESS;
}

/* Get last error message */
const char* cdp_error_last_message(void) {
    static char full_message[512];
    
    if (g_error_state.top < 0) {
        return "No error";
    }
    
    CDPErrorEntry *entry = &g_error_state.stack[g_error_state.top];
    
    if (entry->context[0]) {
        snprintf(full_message, sizeof(full_message), "%s (%s)", 
                entry->message, entry->context);
    } else {
        str_copy_safe(full_message, entry->message, sizeof(full_message));
    }
    
    return full_message;
}

/* Get full error stack trace */
const char* cdp_error_stack_trace(void) {
    static char trace[2048];
    int offset = 0;
    
    if (g_error_state.top < 0) {
        return "No errors in stack";
    }
    
    offset += snprintf(trace + offset, sizeof(trace) - offset, 
                      "Error Stack (newest first):\n");
    
    for (int i = g_error_state.top; i >= 0 && offset < sizeof(trace) - 100; i--) {
        CDPErrorEntry *entry = &g_error_state.stack[i];
        char time_str[32];
        struct tm *tm_info = localtime(&entry->timestamp);
        strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);
        
        offset += snprintf(trace + offset, sizeof(trace) - offset,
                          "  [%s] %s:%d - %s: %s\n",
                          time_str, entry->file, entry->line,
                          error_strings[entry->code], entry->message);
    }
    
    return trace;
}

/* Clear error stack */
void cdp_error_clear(void) {
    g_error_state.top = -1;
    errno = 0;
}

/* Pop last error from stack */
CDPError cdp_error_pop(void) {
    if (g_error_state.top >= 0) {
        CDPError code = g_error_state.stack[g_error_state.top].code;
        g_error_state.top--;
        return code;
    }
    return CDP_SUCCESS;
}

/* Check if error occurred */
int cdp_has_error(void) {
    return g_error_state.top >= 0;
}

/* Get error statistics */
void cdp_error_stats(int *total_errors, int *stack_depth) {
    if (total_errors) *total_errors = g_error_state.total_errors;
    if (stack_depth) *stack_depth = g_error_state.top + 1;
}

/* Helper function to handle common error patterns */
int cdp_check_result(int result, CDPError error_code, const char *operation) {
    if (result < 0) {
        cdp_error_push(error_code, "Operation failed: %s", operation);
        return -1;
    }
    return result;
}

/* Convenience wrapper for null pointer checks */
void* cdp_check_ptr(void *ptr, const char *what) {
    if (!ptr) {
        cdp_error_push(CDP_ERR_MEMORY, "Failed to allocate %s", what);
    }
    return ptr;
}