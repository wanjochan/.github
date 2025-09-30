/**
 * CDP Notify Protocol Service Header
 * Simplified notification handler for notify:// URLs
 */

#ifndef CDP_NOTIFY_H
#define CDP_NOTIFY_H

#include <stddef.h>

/* Compatibility constants */
#define CDP_SYSTEM_SUCCESS 0
#define CDP_NOTIFY_INFO    1
#define CDP_NOTIFY_WARN    2  
#define CDP_NOTIFY_ERROR   3

/* Notify protocol handler */
int cdp_handle_notify_protocol(const char* url, char* response, size_t response_size);

/* Module lifecycle */
int cdp_init_notify_module(void);
int cdp_cleanup_notify_module(void);

/* Compatibility functions - simplified stubs */
const char* cdp_system_error_to_string(int error_code);
int cdp_send_desktop_notification(const char* title, const char* message, int level);
int cdp_generate_html_report(const char* template_file, const char* data_json, const char* output_file);

/* Legacy system module compatibility aliases */
#define cdp_init_system_module() cdp_init_notify_module()
#define cdp_cleanup_system_module() cdp_cleanup_notify_module()

#endif /* CDP_NOTIFY_H */