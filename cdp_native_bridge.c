/**
 * CDP Native Messaging Bridge
 * Connects Chrome Extension to CDP.exe services via Native Messaging protocol
 * 
 * Features:
 * - JavaScript code execution with session management
 * - System command execution with working directory support
 * - File system operations and monitoring
 * - Batch and concurrent operations
 * - Real-time output streaming
 * 
 * Protocol: Chrome Native Messaging (4-byte length + JSON message)
 * Author: AI Assistant
 * Date: 2025-09-10
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>
#include "cdp_internal.h"

/* Constants */
#define MAX_MESSAGE_SIZE 65536
#define MAX_COMMAND_SIZE 4096
#define MAX_PATH_SIZE 512
#define MAX_SESSIONS 32
#define MAX_ENV_VARS 64
#define CDP_DEFAULT_PORT 9222

/* Message types */
#define MSG_EXECUTE_JS "execute_js"
#define MSG_SYSTEM_CMD "system_command"
#define MSG_FILE_OP "file_operation"
#define MSG_BATCH_OP "batch_operation"
#define MSG_SESSION_MGR "session_management"
#define MSG_SCREENSHOT "screenshot"
#define MSG_MONITOR "monitor"

/* Session management */
typedef struct {
    char session_id[64];
    pid_t cdp_process;
    int cdp_port;
    char working_dir[MAX_PATH_SIZE];
    time_t created_time;
    time_t last_used;
    int active;
} cdp_session_t;

/* Request structure */
typedef struct {
    char id[64];
    char type[32];
    char code[MAX_COMMAND_SIZE];
    char session_id[64];
    char working_dir[MAX_PATH_SIZE];
    char options[2048];
    int timeout_ms;
    char env_vars[MAX_ENV_VARS][256];
    int env_count;
} cdp_request_t;

/* Response structure */
typedef struct {
    char id[64];
    int success;
    char result[MAX_MESSAGE_SIZE];
    char error[1024];
    char session_id[64];
    double execution_time_ms;
    int exit_code;
    char stdout_data[MAX_MESSAGE_SIZE];
    char stderr_data[4096];
} cdp_response_t;

/* Global state */
static cdp_session_t sessions[MAX_SESSIONS];
static int session_count = 0;
static pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Forward declarations */
static int read_native_message(char* buffer, size_t buffer_size);
static int send_native_response(const cdp_response_t* response);
static int parse_request(const char* json, cdp_request_t* request);
static int process_request(const cdp_request_t* request, cdp_response_t* response);
static cdp_session_t* get_or_create_session(const char* session_id, const char* working_dir);
static int execute_javascript_command(const cdp_request_t* request, cdp_response_t* response);
static int execute_system_command(const cdp_request_t* request, cdp_response_t* response);
static int execute_file_operation(const cdp_request_t* request, cdp_response_t* response);
static int execute_batch_operation(const cdp_request_t* request, cdp_response_t* response);

/* Read Native Messaging message from Chrome */
static int read_native_message(char* buffer, size_t buffer_size) {
    uint32_t message_length;
    
    // Read 4-byte length prefix (little-endian)
    if (fread(&message_length, 4, 1, stdin) != 1) {
        return -1;  // Connection closed or error
    }
    
    // Validate message length
    if (message_length == 0 || message_length > buffer_size - 1) {
        return -2;  // Invalid length
    }
    
    // Read JSON message body
    if (fread(buffer, message_length, 1, stdin) != 1) {
        return -3;  // Read error
    }
    
    buffer[message_length] = '\0';
    return message_length;
}

/* Send Native Messaging response to Chrome */
static int send_native_response(const cdp_response_t* response) {
    char json_response[MAX_MESSAGE_SIZE];
    
    // Build JSON response
    snprintf(json_response, sizeof(json_response),
        QUOTE({
            "id":"%s",
            "success":%s,
            "result":%s,
            "error":"%s",
            "sessionId":"%s",
            "executionTime":%.2f,
            "exitCode":%d,
            "stdout":"%s",
            "stderr":"%s"
        }),
        response->id,
        response->success ? "true" : "false",
        response->success ? response->result : "null",
        response->error,
        response->session_id,
        response->execution_time_ms,
        response->exit_code,
        response->stdout_data,
        response->stderr_data
    );
    
    uint32_t length = strlen(json_response);
    
    // Send length prefix
    if (fwrite(&length, 4, 1, stdout) != 1) {
        return -1;
    }
    
    // Send JSON message
    if (fwrite(json_response, length, 1, stdout) != 1) {
        return -2;
    }
    
    fflush(stdout);
    return 0;
}

/* Parse JSON request from Chrome */
static int parse_request(const char* json, cdp_request_t* request) {
    // Initialize request structure
    memset(request, 0, sizeof(cdp_request_t));
    request->timeout_ms = 30000;  // Default 30 seconds
    strcpy(request->working_dir, ".");  // Default current directory
    
    // Simple JSON parsing (can be enhanced with a proper JSON library)
    const char* id_start = strstr(json, JKEYQ("id"));
    if (id_start) {
        id_start += 6;
        const char* id_end = strchr(id_start, '"');
        if (id_end) {
            strncpy(request->id, id_start, MIN(id_end - id_start, sizeof(request->id) - 1));
        }
    }
    
    const char* type_start = strstr(json, JKEYQ("type"));
    if (type_start) {
        type_start += 8;
        const char* type_end = strchr(type_start, '"');
        if (type_end) {
            strncpy(request->type, type_start, MIN(type_end - type_start, sizeof(request->type) - 1));
        }
    }
    
    const char* code_start = strstr(json, JKEYQ("code"));
    if (code_start) {
        code_start += 8;
        const char* code_end = strchr(code_start, '"');
        if (code_end) {
            strncpy(request->code, code_start, MIN(code_end - code_start, sizeof(request->code) - 1));
        }
    }
    
    const char* session_start = strstr(json, JKEYQ("sessionId"));
    if (session_start) {
        session_start += 13;
        const char* session_end = strchr(session_start, '"');
        if (session_end) {
            strncpy(request->session_id, session_start, MIN(session_end - session_start, sizeof(request->session_id) - 1));
        }
    }
    
    const char* timeout_start = strstr(json, JKEY("timeout"));
    if (timeout_start) {
        request->timeout_ms = atoi(timeout_start + 10);
    }
    
    return 0;
}

/* Get or create CDP session */
static cdp_session_t* get_or_create_session(const char* session_id, const char* working_dir) {
    pthread_mutex_lock(&session_mutex);
    
    // Look for existing session
    for (int i = 0; i < session_count; i++) {
        if (sessions[i].active && strcmp(sessions[i].session_id, session_id) == 0) {
            sessions[i].last_used = time(NULL);
            pthread_mutex_unlock(&session_mutex);
            return &sessions[i];
        }
    }
    
    // Create new session
    if (session_count >= MAX_SESSIONS) {
        pthread_mutex_unlock(&session_mutex);
        return NULL;  // Too many sessions
    }
    
    cdp_session_t* session = &sessions[session_count++];
    strcpy(session->session_id, session_id);
    strcpy(session->working_dir, working_dir);
    session->created_time = time(NULL);
    session->last_used = time(NULL);
    session->active = 1;
    session->cdp_port = CDP_DEFAULT_PORT + session_count;  // Use different ports
    
    // Launch CDP instance for this session
    char cdp_command[1024];
    snprintf(cdp_command, sizeof(cdp_command), 
        "./cdp.exe --service --port %d &", session->cdp_port);
    
    session->cdp_process = system(cdp_command);
    
    pthread_mutex_unlock(&session_mutex);
    return session;
}

/* Execute JavaScript command via CDP */
static int execute_javascript_command(const cdp_request_t* request, cdp_response_t* response) {
    cdp_session_t* session = get_or_create_session(request->session_id, request->working_dir);
    if (!session) {
        strcpy(response->error, "Failed to create session");
        return -1;
    }
    
    strcpy(response->session_id, session->session_id);
    
    // Execute JavaScript via CDP
    char cdp_command[8192];
    snprintf(cdp_command, sizeof(cdp_command),
        "echo '%s' | ./cdp.exe -d %d 2>&1",
        request->code, session->cdp_port);
    
    clock_t start_time = clock();
    
    FILE* fp = popen(cdp_command, "r");
    if (!fp) {
        strcpy(response->error, "Failed to execute CDP command");
        return -2;
    }
    
    // Read result
    char result_buffer[MAX_MESSAGE_SIZE] = {0};
    size_t total_read = 0;
    
    while (fgets(result_buffer + total_read, 
                 sizeof(result_buffer) - total_read - 1, fp) && 
           total_read < sizeof(result_buffer) - 1) {
        total_read = strlen(result_buffer);
    }
    
    int exit_code = pclose(fp);
    
    clock_t end_time = clock();
    response->execution_time_ms = ((double)(end_time - start_time) / CLOCKS_PER_SEC) * 1000.0;
    response->exit_code = exit_code;
    
    if (exit_code == 0) {
        strcpy(response->result, result_buffer);
        response->success = 1;
    } else {
        strcpy(response->error, result_buffer);
        response->success = 0;
    }
    
    return 0;
}

/* Execute system command */
static int execute_system_command(const cdp_request_t* request, cdp_response_t* response) {
    cdp_session_t* session = get_or_create_session(request->session_id, request->working_dir);
    if (!session) {
        strcpy(response->error, "Failed to create session");
        return -1;
    }
    
    strcpy(response->session_id, session->session_id);
    
    // Change to working directory
    char original_dir[MAX_PATH_SIZE];
    getcwd(original_dir, sizeof(original_dir));
    
    if (chdir(session->working_dir) != 0) {
        snprintf(response->error, sizeof(response->error), 
            "Failed to change to directory: %s", session->working_dir);
        return -2;
    }
    
    clock_t start_time = clock();
    
    // Execute system command
    FILE* fp = popen(request->code, "r");
    if (!fp) {
        chdir(original_dir);
        strcpy(response->error, "Failed to execute system command");
        return -3;
    }
    
    // Read stdout
    char stdout_buffer[MAX_MESSAGE_SIZE] = {0};
    size_t total_read = 0;
    
    while (fgets(stdout_buffer + total_read,
                 sizeof(stdout_buffer) - total_read - 1, fp) &&
           total_read < sizeof(stdout_buffer) - 1) {
        total_read = strlen(stdout_buffer);
    }
    
    int exit_code = pclose(fp);
    chdir(original_dir);
    
    clock_t end_time = clock();
    response->execution_time_ms = ((double)(end_time - start_time) / CLOCKS_PER_SEC) * 1000.0;
    response->exit_code = exit_code;
    
    if (exit_code == 0) {
        strcpy(response->result, QUOTE("success"));
        strcpy(response->stdout_data, stdout_buffer);
        response->success = 1;
    } else {
        strcpy(response->error, "Command execution failed");
        strcpy(response->stderr_data, stdout_buffer);
        response->success = 0;
    }
    
    return 0;
}

/* Execute file operations */
static int execute_file_operation(const cdp_request_t* request, cdp_response_t* response) {
    strcpy(response->session_id, request->session_id);
    
    // Parse file operation from options
    const char* operation = strstr(request->options, JKEYQ("operation"));
    if (!operation) {
        strcpy(response->error, "Missing operation parameter");
        return -1;
    }
    
    operation += 13;  // Skip past "operation":"
    
    clock_t start_time = clock();
    
    if (strncmp(operation, "screenshot", 10) == 0) {
        // Take screenshot using CDP
        char screenshot_cmd[2048];
        snprintf(screenshot_cmd, sizeof(screenshot_cmd),
            "echo 'Page.captureScreenshot({})' | ./cdp.exe -d %d 2>&1",
            CDP_DEFAULT_PORT);
        
        FILE* fp = popen(screenshot_cmd, "r");
        if (!fp) {
            strcpy(response->error, "Screenshot command failed");
            return -2;
        }
        
        char result_buffer[MAX_MESSAGE_SIZE] = {0};
        fread(result_buffer, 1, sizeof(result_buffer) - 1, fp);
        pclose(fp);
        
        strcpy(response->result, result_buffer);
        response->success = 1;
        
    } else if (strncmp(operation, "monitor_downloads", 17) == 0) {
        // Start download monitoring
        char monitor_cmd[1024];
        snprintf(monitor_cmd, sizeof(monitor_cmd),
            "./cdp.exe --monitor-downloads %s --timeout %d &",
            request->working_dir, request->timeout_ms);
        
        int result = system(monitor_cmd);
        if (result == 0) {
            strcpy(response->result, QUOTE("monitoring_started"));
            response->success = 1;
        } else {
            strcpy(response->error, "Failed to start monitoring");
            response->success = 0;
        }
        
    } else {
        strcpy(response->error, "Unknown file operation");
        return -3;
    }
    
    clock_t end_time = clock();
    response->execution_time_ms = ((double)(end_time - start_time) / CLOCKS_PER_SEC) * 1000.0;
    
    return 0;
}

/* Execute batch operations */
static int execute_batch_operation(const cdp_request_t* request, cdp_response_t* response) {
    strcpy(response->session_id, request->session_id);
    
    // Parse batch commands from options
    char batch_result[MAX_MESSAGE_SIZE] = "[";
    int first = 1;
    
    clock_t start_time = clock();
    
    // Simple batch processing (can be enhanced for true parallelism)
    const char* commands_start = strstr(request->options, JKEY("commands") "[");
    if (!commands_start) {
        strcpy(response->error, "Missing commands array");
        return -1;
    }
    
    // For simplicity, execute commands sequentially
    // In a full implementation, this would use the concurrent module
    char temp_cmd[2048];
    snprintf(temp_cmd, sizeof(temp_cmd),
        "echo '[%s]' | ./cdp.exe --batch-mode -d %d 2>&1",
        request->code, CDP_DEFAULT_PORT);
    
    FILE* fp = popen(temp_cmd, "r");
    if (!fp) {
        strcpy(response->error, "Batch execution failed");
        return -2;
    }
    
    char result_buffer[MAX_MESSAGE_SIZE] = {0};
    fread(result_buffer, 1, sizeof(result_buffer) - 1, fp);
    int exit_code = pclose(fp);
    
    clock_t end_time = clock();
    response->execution_time_ms = ((double)(end_time - start_time) / CLOCKS_PER_SEC) * 1000.0;
    response->exit_code = exit_code;
    
    if (exit_code == 0) {
        strcpy(response->result, result_buffer);
        response->success = 1;
    } else {
        strcpy(response->error, result_buffer);
        response->success = 0;
    }
    
    return 0;
}

/* Process incoming request */
static int process_request(const cdp_request_t* request, cdp_response_t* response) {
    // Initialize response
    memset(response, 0, sizeof(cdp_response_t));
    strcpy(response->id, request->id);
    strcpy(response->session_id, request->session_id);
    
    // Route to appropriate handler based on message type
    if (strcmp(request->type, MSG_EXECUTE_JS) == 0) {
        return execute_javascript_command(request, response);
    } else if (strcmp(request->type, MSG_SYSTEM_CMD) == 0) {
        return execute_system_command(request, response);
    } else if (strcmp(request->type, MSG_FILE_OP) == 0) {
        return execute_file_operation(request, response);
    } else if (strcmp(request->type, MSG_BATCH_OP) == 0) {
        return execute_batch_operation(request, response);
    } else {
        snprintf(response->error, sizeof(response->error), 
            "Unknown message type: %s", request->type);
        return -1;
    }
}

/* Main Native Messaging loop */
int main(int argc, char* argv[]) {
    char input_buffer[MAX_MESSAGE_SIZE];
    cdp_request_t request;
    cdp_response_t response;
    
    // Initialize session management
    memset(sessions, 0, sizeof(sessions));
    
    // Set binary mode for stdin/stdout
    #ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
    #endif
    
    // Main message processing loop
    while (1) {
        // Read message from Chrome Extension
        int msg_len = read_native_message(input_buffer, sizeof(input_buffer));
        if (msg_len < 0) {
            break;  // Connection closed or error
        }
        
        // Parse JSON request
        if (parse_request(input_buffer, &request) < 0) {
            // Send error response
            memset(&response, 0, sizeof(response));
            strcpy(response.error, "Invalid request format");
            send_native_response(&response);
            continue;
        }
        
        // Process request
        if (process_request(&request, &response) < 0) {
            // Error already set in response structure
        }
        
        // Send response back to Chrome
        send_native_response(&response);
    }
    
    // Cleanup: terminate all CDP sessions
    pthread_mutex_lock(&session_mutex);
    for (int i = 0; i < session_count; i++) {
        if (sessions[i].active && sessions[i].cdp_process > 0) {
            kill(sessions[i].cdp_process, SIGTERM);
        }
    }
    pthread_mutex_unlock(&session_mutex);
    
    return 0;
}

/* Utility function: MIN macro */
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
