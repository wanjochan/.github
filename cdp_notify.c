/**
 * CDP Notify Protocol Service Implementation
 * Handles notify:// protocol for system notifications
 * Simplified from previous system integration module
 */

#include "cdp_notify.h" 
#include "cdp_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Simple notify protocol handler */
int cdp_handle_notify_protocol(const char* url, char* response, size_t response_size) {
    if (!url || !response || response_size == 0) {
        return -1;
    }
    
    /* Extract message from notify://title?message=content&level=info */
    if (strncmp(url, "notify://", 9) != 0) {
        snprintf(response, response_size, QUOTE({"error": "Invalid notify protocol URL"}));
        return -1;
    }
    
    const char* title_start = url + 9;
    const char* params_start = strchr(title_start, '?');
    
    char title[256] = "Notification";
    if (params_start) {
        size_t title_len = params_start - title_start;
        if (title_len > 0 && title_len < sizeof(title)) {
            strncpy(title, title_start, title_len);
            title[title_len] = '\0';
        }
    } else {
        strncpy(title, title_start, sizeof(title) - 1);
        title[sizeof(title) - 1] = '\0';
    }
    
    /* Simple console notification for now */
    cdp_log(CDP_LOG_INFO, "NOTIFY", "🔔 %s", title);
    if (params_start && strstr(params_start, "message=")) {
        char* msg_start = strstr(params_start, "message=") + 8;
        char* msg_end = strchr(msg_start, '&');
        if (!msg_end) msg_end = msg_start + strlen(msg_start);
        
        char message[512];
        size_t msg_len = msg_end - msg_start;
        if (msg_len < sizeof(message)) {
            strncpy(message, msg_start, msg_len);
            message[msg_len] = '\0';
            cdp_log(CDP_LOG_INFO, "NOTIFY", "   %s", message);
        }
    }
    
    snprintf(response, response_size, QUOTE({"ok": true, "title": "%s"}), title);
    return 0;
}

/* Initialize notify protocol service */
int cdp_init_notify_module(void) {
    /* Simple notification service - no complex initialization */
    return CDP_SYSTEM_SUCCESS;
}

/* Cleanup notify protocol service */
int cdp_cleanup_notify_module(void) {
    /* No cleanup needed for simple notification handler */
    return 0;
}

/* Compatibility stubs for removed system functions */
const char* cdp_system_error_to_string(int error_code) {
    return error_code == CDP_SYSTEM_SUCCESS ? "Success" : "Error";
}

int cdp_send_desktop_notification(const char* title, const char* message, int level) {
    /* Simplified: just print to console */
    cdp_log(CDP_LOG_INFO, "NOTIFY", "🔔 %s: %s", title ? title : "Notification", message ? message : "");
    return 0;
}

int cdp_generate_html_report(const char* template_file, const char* data_json, const char* output_file) {
    /* Simplified: suggest using cli:// protocol instead */
    cdp_log(CDP_LOG_INFO, "NOTIFY", "HTML report generation moved to cli:// protocol - use cli://pandoc or similar");
    return -1;
}