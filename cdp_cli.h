/**
 * CDP CLI Protocol Service Header
 * Simplified protocol handler for cli:// URLs
 */

#ifndef CDP_CLI_H
#define CDP_CLI_H

#include <stddef.h>

/* Compatibility constants */
#define CDP_FILE_SUCCESS 0

/* CLI protocol handler */
int cdp_handle_cli_protocol(const char* url, char* response, size_t response_size);

/* Module lifecycle */
int cdp_init_cli_module(void);
int cdp_cleanup_cli_module(void);

/* Minimal filesystem functions for compatibility */
int cdp_validate_file_exists(const char* file_path);

/* Compatibility functions - simplified stubs */
const char* cdp_file_error_to_string(int error_code);
int cdp_start_download_monitor(const char* dir);

#endif /* CDP_CLI_H */