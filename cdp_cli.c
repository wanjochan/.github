/**
 * CDP CLI Protocol Service Implementation
 * Handles cli:// protocol for system command execution
 * Simplified from previous filesystem abstraction - focus on protocol bridging
 */

#include "cdp_cli.h"
#include "cdp_internal.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

/* Simple CLI protocol handler */
int cdp_handle_cli_protocol(const char* url, char* response, size_t response_size) {
    if (!url || !response || response_size == 0) {
        return -1;
    }
    
    /* Extract command from cli://command?params */
    if (strncmp(url, "cli://", 6) != 0) {
        snprintf(response, response_size, QUOTE({"error": "Invalid CLI protocol URL"}));
        return -1;
    }
    
    const char* cmd_start = url + 6;
    const char* params_start = strchr(cmd_start, '?');
    
    char command[1024];
    if (params_start) {
        size_t cmd_len = params_start - cmd_start;
        if (cmd_len >= sizeof(command)) cmd_len = sizeof(command) - 1;
        strncpy(command, cmd_start, cmd_len);
        command[cmd_len] = '\0';
    } else {
        strncpy(command, cmd_start, sizeof(command) - 1);
        command[sizeof(command) - 1] = '\0';
    }
    
    /* Basic security: whitelist common safe commands */
    if (strncmp(command, "ls", 2) == 0 || 
        strncmp(command, "pwd", 3) == 0 ||
        strncmp(command, "echo", 4) == 0 ||
        strncmp(command, "date", 4) == 0 ||
        strncmp(command, "whoami", 6) == 0) {
        
        /* Execute command safely */
        FILE* pipe = popen(command, "r");
        if (!pipe) {
            snprintf(response, response_size, QUOTE({"error": "Command execution failed"}));
            return -1;
        }
        
        char output[4096] = {0};
        fread(output, 1, sizeof(output) - 1, pipe);
        int exit_code = pclose(pipe);
        
        /* Remove trailing newline */
        size_t len = strlen(output);
        if (len > 0 && output[len-1] == '\n') {
            output[len-1] = '\0';
        }
        
        snprintf(response, response_size, QUOTE({"ok": true, "output": "%s", "exit_code": %d}), 
                output, WEXITSTATUS(exit_code));
        return 0;
    }
    
    snprintf(response, response_size, QUOTE({"error": "Command not allowed"}));
    return -1;
}

/* Initialize CLI protocol service */
int cdp_init_cli_module(void) {
    /* No complex initialization needed - just protocol handling */
    return 0;
}

/* Cleanup CLI protocol service */
int cdp_cleanup_cli_module(void) {
    /* No cleanup needed for simple protocol handler */
    return 0;
}

/* File exists check - minimal filesystem function */
int cdp_validate_file_exists(const char* file_path) {
    if (!file_path) return 0;
    return access(file_path, F_OK) == 0 ? 1 : 0;
}

/* Compatibility stubs for removed filesystem functions */
const char* cdp_file_error_to_string(int error_code) {
    return error_code == 0 ? "Success" : "Error";
}

int cdp_start_download_monitor(const char* dir) {
    /* Simplified: just return success - no complex monitoring needed */
    return 0;
}