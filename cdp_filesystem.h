/**
 * CDP Filesystem Interaction Module
 * Enhanced file system operations that Chrome cannot perform due to security sandbox
 * 
 * This module provides system-level file operations including:
 * - Download monitoring and notification
 * - File upload automation
 * - Batch file processing (screenshots, content extraction)
 * - File system utilities (create, move, compress, cleanup)
 * 
 * Author: AI Assistant
 * Date: 2025-09-10
 * Task: CDP_TASK_250910A - Task 2: Filesystem Interaction Features
 */

#ifndef CDP_FILESYSTEM_H
#define CDP_FILESYSTEM_H

#include <sys/types.h>
#include <time.h>
#include <pthread.h>

/* Constants */
#define CDP_MAX_FILENAME_LENGTH 256
#define CDP_MAX_FILEPATH_LENGTH 512
#define CDP_MAX_URL_LENGTH 512
#define CDP_MAX_MIME_TYPE_LENGTH 128
#define CDP_MAX_DOWNLOAD_MONITORS 16
#define CDP_MAX_BATCH_TASKS 100
#define CDP_DEFAULT_DOWNLOAD_TIMEOUT 30000  /* 30 seconds */
#define CDP_MAX_FILE_SIZE_MB 1024  /* 1GB max file size */

/* Download status */
typedef enum {
    CDP_DOWNLOAD_STATUS_UNKNOWN = 0,
    CDP_DOWNLOAD_STATUS_STARTING,
    CDP_DOWNLOAD_STATUS_DOWNLOADING,
    CDP_DOWNLOAD_STATUS_COMPLETED,
    CDP_DOWNLOAD_STATUS_FAILED,
    CDP_DOWNLOAD_STATUS_CANCELLED
} cdp_download_status_t;

/* File operation result */
typedef enum {
    CDP_FILE_SUCCESS = 0,
    CDP_FILE_ERR_NOT_FOUND = -2000,
    CDP_FILE_ERR_PERMISSION_DENIED = -2001,
    CDP_FILE_ERR_INVALID_PATH = -2002,
    CDP_FILE_ERR_DISK_FULL = -2003,
    CDP_FILE_ERR_FILE_TOO_LARGE = -2004,
    CDP_FILE_ERR_TIMEOUT = -2005,
    CDP_FILE_ERR_INVALID_FORMAT = -2006,
    CDP_FILE_ERR_NETWORK_ERROR = -2007,
    CDP_FILE_ERR_MONITOR_FAILED = -2008,
    CDP_FILE_ERR_UPLOAD_FAILED = -2009,
    CDP_FILE_ERR_MEMORY = -2010
} cdp_file_error_t;

/* Download information structure */
typedef struct {
    char filename[CDP_MAX_FILENAME_LENGTH];
    char full_path[CDP_MAX_FILEPATH_LENGTH];
    char url[CDP_MAX_URL_LENGTH];
    size_t file_size;
    size_t downloaded_size;
    time_t start_time;
    time_t completion_time;
    cdp_download_status_t status;
    double download_speed;  /* bytes per second */
    char mime_type[CDP_MAX_MIME_TYPE_LENGTH];
    char error_message[256];
} cdp_download_info_t;

/* Screenshot task structure */
typedef struct {
    char url[CDP_MAX_URL_LENGTH];
    char output_file[CDP_MAX_FILEPATH_LENGTH];
    int width;
    int height;
    int quality;  /* 1-100 for JPEG quality */
    int full_page;  /* 0=viewport only, 1=full page */
    int success;
    time_t start_time;
    time_t completion_time;
    char error_message[256];
} cdp_screenshot_task_t;

/* File upload task structure */
typedef struct {
    char local_file_path[CDP_MAX_FILEPATH_LENGTH];
    char target_selector[256];
    char mime_type[CDP_MAX_MIME_TYPE_LENGTH];
    size_t file_size;
    int success;
    time_t upload_time;
    char error_message[256];
} cdp_upload_task_t;

/* Download monitor structure */
typedef struct {
    char watch_directory[CDP_MAX_FILEPATH_LENGTH];
    char filename_pattern[256];
    int active;
    pthread_t monitor_thread;
    pthread_mutex_t mutex;
    void (*callback)(const cdp_download_info_t* download);
    void* callback_data;
    time_t last_check;
    int check_interval_ms;
} cdp_download_monitor_t;

/* File operation statistics */
typedef struct {
    int total_downloads_monitored;
    int successful_downloads;
    int failed_downloads;
    int total_uploads;
    int successful_uploads;
    int failed_uploads;
    int total_screenshots;
    int successful_screenshots;
    int failed_screenshots;
    size_t total_bytes_processed;
    time_t start_time;
} cdp_file_stats_t;

/* Download callback function type */
typedef void (*cdp_download_callback_t)(const cdp_download_info_t* download, void* user_data);

/* Core API Functions */

/* Download Monitoring */
int cdp_start_download_monitor(const char* watch_directory);
int cdp_stop_download_monitor(void);
int cdp_wait_for_download(const char* filename_pattern, int timeout_ms, cdp_download_info_t* info);
int cdp_get_download_list(cdp_download_info_t** downloads, int* count);
int cdp_set_download_callback(cdp_download_callback_t callback, void* user_data);
int cdp_check_download_status(const char* filename, cdp_download_info_t* info);

/* File Upload Automation */
int cdp_upload_file_to_element(const char* selector, const char* local_file_path);
int cdp_batch_upload_files(const char* selector, const char** file_paths, int count);
int cdp_drag_drop_file(const char* target_selector, const char* file_path);
int cdp_validate_file_exists(const char* file_path);
int cdp_get_file_mime_type(const char* file_path, char* mime_type, size_t size);
int cdp_get_file_size(const char* file_path, size_t* size);

/* Batch File Processing */
int cdp_batch_screenshot(cdp_screenshot_task_t* tasks, int count);
int cdp_screenshot_with_options(const char* url, const char* output_file, int width, int height);
int cdp_save_page_html(const char* output_file);
int cdp_save_page_pdf(const char* output_file, const char* options);
int cdp_extract_page_text(char* output_buffer, size_t buffer_size);
int cdp_save_page_mhtml(const char* output_file);

/* File System Operations */
int cdp_create_directory(const char* dir_path);
int cdp_cleanup_temp_files(const char* temp_dir);
int cdp_compress_files(const char** file_paths, int count, const char* output_zip);
int cdp_move_file(const char* src_path, const char* dst_path);
int cdp_copy_file(const char* src_path, const char* dst_path);
int cdp_delete_file(const char* file_path);
int cdp_get_directory_size(const char* dir_path, size_t* total_size);

/* Advanced File Operations */
int cdp_watch_directory_changes(const char* dir_path, void (*callback)(const char* path, int event_type));
int cdp_create_file_backup(const char* file_path, const char* backup_suffix);
int cdp_verify_file_integrity(const char* file_path, const char* expected_hash);
int cdp_get_file_metadata(const char* file_path, time_t* created, time_t* modified, size_t* size);

/* Utility Functions */
int cdp_is_valid_filename(const char* filename);
int cdp_sanitize_filename(const char* input, char* output, size_t output_size);
int cdp_get_temp_directory(char* temp_dir, size_t size);
int cdp_get_downloads_directory(char* downloads_dir, size_t size);
const char* cdp_download_status_to_string(cdp_download_status_t status);
const char* cdp_file_error_to_string(cdp_file_error_t error);

/* Statistics and Monitoring */
int cdp_get_file_stats(cdp_file_stats_t* stats);
int cdp_reset_file_stats(void);
int cdp_export_file_stats(const char* output_file, const char* format);

/* Configuration */
int cdp_set_download_timeout(int timeout_ms);
int cdp_set_max_file_size(size_t max_size_mb);
int cdp_set_temp_directory(const char* temp_dir);
int cdp_enable_file_logging(int enable);

/* Initialization and Cleanup */
int cdp_init_filesystem_module(void);
int cdp_cleanup_filesystem_module(void);

/* Global filesystem module state */
extern cdp_file_stats_t g_file_stats;

#endif /* CDP_FILESYSTEM_H */
