/**
 * CDP Filesystem Interaction Module Implementation
 * Enhanced file system operations beyond Chrome's security sandbox
 */

#include "cdp_filesystem.h"
#include "cdp_internal.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/* Global state */
static int filesystem_initialized = 0;
static cdp_download_monitor_t download_monitors[CDP_MAX_DOWNLOAD_MONITORS];
static int active_monitors = 0;
static pthread_mutex_t filesystem_mutex = PTHREAD_MUTEX_INITIALIZER;
static cdp_download_callback_t global_download_callback = NULL;
static void* global_callback_data = NULL;

/* Configuration */
static int download_timeout_ms = CDP_DEFAULT_DOWNLOAD_TIMEOUT;
static size_t max_file_size_mb = CDP_MAX_FILE_SIZE_MB;
static char temp_directory[CDP_MAX_FILEPATH_LENGTH] = {0};
static int file_logging_enabled = 0;

/* Global statistics */
cdp_file_stats_t g_file_stats = {0};

/* Forward declarations */
static void* download_monitor_thread(void* arg);
static int scan_directory_for_downloads(const char* dir_path, cdp_download_info_t** downloads, int* count);
static int is_download_complete(const char* file_path);
static void update_file_stats(int operation_type, int success, size_t bytes);

/* Initialize filesystem module */
int cdp_init_filesystem_module(void) {
    if (filesystem_initialized) {
        return CDP_FILE_SUCCESS;
    }
    
    pthread_mutex_lock(&filesystem_mutex);
    
    /* Initialize download monitors */
    memset(download_monitors, 0, sizeof(download_monitors));
    active_monitors = 0;
    
    /* Initialize statistics */
    memset(&g_file_stats, 0, sizeof(g_file_stats));
    g_file_stats.start_time = time(NULL);
    
    /* Set default temp directory */
    if (strlen(temp_directory) == 0) {
        const char* tmp = getenv("TMPDIR");
        if (!tmp) tmp = getenv("TMP");
        if (!tmp) tmp = "/tmp";
        snprintf(temp_directory, sizeof(temp_directory), "%s/cdp_files", tmp);
        
        /* Create temp directory if it doesn't exist */
        mkdir(temp_directory, 0755);
    }
    
    filesystem_initialized = 1;
    
    pthread_mutex_unlock(&filesystem_mutex);
    
    return CDP_FILE_SUCCESS;
}

/* Cleanup filesystem module */
int cdp_cleanup_filesystem_module(void) {
    if (!filesystem_initialized) {
        return CDP_FILE_SUCCESS;
    }
    
    pthread_mutex_lock(&filesystem_mutex);
    
    /* Stop all download monitors */
    for (int i = 0; i < CDP_MAX_DOWNLOAD_MONITORS; i++) {
        if (download_monitors[i].active) {
            download_monitors[i].active = 0;
            pthread_join(download_monitors[i].monitor_thread, NULL);
            pthread_mutex_destroy(&download_monitors[i].mutex);
        }
    }
    
    active_monitors = 0;
    filesystem_initialized = 0;
    
    pthread_mutex_unlock(&filesystem_mutex);
    
    return CDP_FILE_SUCCESS;
}

/* Start download monitoring */
int cdp_start_download_monitor(const char* watch_directory) {
    if (!watch_directory) {
        return CDP_FILE_ERR_INVALID_PATH;
    }
    
    if (!filesystem_initialized) {
        int ret = cdp_init_filesystem_module();
        if (ret != CDP_FILE_SUCCESS) {
            return ret;
        }
    }
    
    /* Check if directory exists */
    struct stat st;
    if (stat(watch_directory, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return CDP_FILE_ERR_NOT_FOUND;
    }
    
    pthread_mutex_lock(&filesystem_mutex);
    
    /* Find available monitor slot */
    cdp_download_monitor_t* monitor = NULL;
    for (int i = 0; i < CDP_MAX_DOWNLOAD_MONITORS; i++) {
        if (!download_monitors[i].active) {
            monitor = &download_monitors[i];
            break;
        }
    }
    
    if (!monitor) {
        pthread_mutex_unlock(&filesystem_mutex);
        return CDP_FILE_ERR_MONITOR_FAILED;
    }
    
    /* Initialize monitor */
    memset(monitor, 0, sizeof(cdp_download_monitor_t));
    strncpy(monitor->watch_directory, watch_directory, sizeof(monitor->watch_directory) - 1);
    monitor->active = 1;
    monitor->check_interval_ms = 1000;  /* Check every second */
    monitor->last_check = time(NULL);
    
    if (pthread_mutex_init(&monitor->mutex, NULL) != 0) {
        pthread_mutex_unlock(&filesystem_mutex);
        return CDP_FILE_ERR_MEMORY;
    }
    
    /* Start monitor thread */
    if (pthread_create(&monitor->monitor_thread, NULL, download_monitor_thread, monitor) != 0) {
        pthread_mutex_destroy(&monitor->mutex);
        monitor->active = 0;
        pthread_mutex_unlock(&filesystem_mutex);
        return CDP_FILE_ERR_MONITOR_FAILED;
    }
    
    active_monitors++;
    
    pthread_mutex_unlock(&filesystem_mutex);
    
    return CDP_FILE_SUCCESS;
}

/* Stop download monitoring */
int cdp_stop_download_monitor(void) {
    if (!filesystem_initialized) {
        return CDP_FILE_SUCCESS;
    }
    
    pthread_mutex_lock(&filesystem_mutex);
    
    /* Stop all active monitors */
    for (int i = 0; i < CDP_MAX_DOWNLOAD_MONITORS; i++) {
        if (download_monitors[i].active) {
            download_monitors[i].active = 0;
            pthread_join(download_monitors[i].monitor_thread, NULL);
            pthread_mutex_destroy(&download_monitors[i].mutex);
        }
    }
    
    active_monitors = 0;
    
    pthread_mutex_unlock(&filesystem_mutex);
    
    return CDP_FILE_SUCCESS;
}

/* Download monitor thread */
static void* download_monitor_thread(void* arg) {
    cdp_download_monitor_t* monitor = (cdp_download_monitor_t*)arg;
    
    while (monitor->active) {
        pthread_mutex_lock(&monitor->mutex);
        
        /* Scan directory for new downloads */
        cdp_download_info_t* downloads = NULL;
        int count = 0;
        
        if (scan_directory_for_downloads(monitor->watch_directory, &downloads, &count) == CDP_FILE_SUCCESS) {
            /* Check each download */
            for (int i = 0; i < count; i++) {
                if (downloads[i].status == CDP_DOWNLOAD_STATUS_COMPLETED) {
                    /* Trigger callback if set */
                    if (monitor->callback) {
                        monitor->callback(&downloads[i]);
                    } else if (global_download_callback) {
                        global_download_callback(&downloads[i], global_callback_data);
                    }
                    
                    /* Update statistics */
                    update_file_stats(0, 1, downloads[i].file_size);
                }
            }
            
            if (downloads) {
                free(downloads);
            }
        }
        
        monitor->last_check = time(NULL);
        
        pthread_mutex_unlock(&monitor->mutex);
        
        /* Sleep for check interval */
        usleep(monitor->check_interval_ms * 1000);
    }
    
    return NULL;
}

/* Scan directory for downloads */
static int scan_directory_for_downloads(const char* dir_path, cdp_download_info_t** downloads, int* count) {
    DIR* dir = opendir(dir_path);
    if (!dir) {
        return CDP_FILE_ERR_NOT_FOUND;
    }
    
    /* Count files first */
    int file_count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_REG) {
            file_count++;
        }
    }
    
    if (file_count == 0) {
        closedir(dir);
        *downloads = NULL;
        *count = 0;
        return CDP_FILE_SUCCESS;
    }
    
    /* Allocate memory for downloads */
    cdp_download_info_t* download_list = malloc(file_count * sizeof(cdp_download_info_t));
    if (!download_list) {
        closedir(dir);
        return CDP_FILE_ERR_MEMORY;
    }
    
    /* Reset directory position */
    rewinddir(dir);
    
    int download_index = 0;
    while ((entry = readdir(dir)) != NULL && download_index < file_count) {
        if (entry->d_type != DT_REG) {
            continue;
        }
        
        cdp_download_info_t* download = &download_list[download_index];
        memset(download, 0, sizeof(cdp_download_info_t));
        
        /* Set filename */
        strncpy(download->filename, entry->d_name, sizeof(download->filename) - 1);
        
        /* Set full path */
        snprintf(download->full_path, sizeof(download->full_path), "%s/%s", dir_path, entry->d_name);
        
        /* Get file stats */
        struct stat st;
        if (stat(download->full_path, &st) == 0) {
            download->file_size = st.st_size;
            download->completion_time = st.st_mtime;
            
            /* Check if download is complete */
            if (is_download_complete(download->full_path)) {
                download->status = CDP_DOWNLOAD_STATUS_COMPLETED;
            } else {
                download->status = CDP_DOWNLOAD_STATUS_DOWNLOADING;
            }
        } else {
            download->status = CDP_DOWNLOAD_STATUS_FAILED;
        }
        
        download_index++;
    }
    
    closedir(dir);
    
    *downloads = download_list;
    *count = download_index;
    
    return CDP_FILE_SUCCESS;
}

/* Check if download is complete */
static int is_download_complete(const char* file_path) {
    /* Simple heuristic: check if file has .crdownload or .part extension */
    const char* ext = strrchr(file_path, '.');
    if (ext) {
        if (strcmp(ext, ".crdownload") == 0 || strcmp(ext, ".part") == 0) {
            return 0;  /* Still downloading */
        }
    }
    
    /* Check if file can be opened for reading (not locked by downloader) */
    int fd = open(file_path, O_RDONLY);
    if (fd >= 0) {
        close(fd);
        return 1;  /* Complete */
    }
    
    return 0;  /* Probably still downloading */
}

/* Update file statistics */
static void update_file_stats(int operation_type, int success, size_t bytes) {
    pthread_mutex_lock(&filesystem_mutex);

    switch (operation_type) {
        case 0:  /* Download */
            g_file_stats.total_downloads_monitored++;
            if (success) {
                g_file_stats.successful_downloads++;
            } else {
                g_file_stats.failed_downloads++;
            }
            break;
        case 1:  /* Upload */
            g_file_stats.total_uploads++;
            if (success) {
                g_file_stats.successful_uploads++;
            } else {
                g_file_stats.failed_uploads++;
            }
            break;
        case 2:  /* Screenshot */
            g_file_stats.total_screenshots++;
            if (success) {
                g_file_stats.successful_screenshots++;
            } else {
                g_file_stats.failed_screenshots++;
            }
            break;
    }

    if (success) {
        g_file_stats.total_bytes_processed += bytes;
    }

    pthread_mutex_unlock(&filesystem_mutex);
}

/* Set download callback */
int cdp_set_download_callback(cdp_download_callback_t callback, void* user_data) {
    pthread_mutex_lock(&filesystem_mutex);
    global_download_callback = callback;
    global_callback_data = user_data;
    pthread_mutex_unlock(&filesystem_mutex);
    return CDP_FILE_SUCCESS;
}

/* Wait for download */
int cdp_wait_for_download(const char* filename_pattern, int timeout_ms, cdp_download_info_t* info) {
    if (!filename_pattern || !info) {
        return CDP_FILE_ERR_INVALID_PATH;
    }

    if (!filesystem_initialized) {
        return CDP_FILE_ERR_MONITOR_FAILED;
    }

    time_t start_time = time(NULL);
    time_t timeout_sec = timeout_ms / 1000;

    while (time(NULL) - start_time < timeout_sec) {
        /* Check all monitored directories */
        pthread_mutex_lock(&filesystem_mutex);

        for (int i = 0; i < CDP_MAX_DOWNLOAD_MONITORS; i++) {
            if (!download_monitors[i].active) continue;

            cdp_download_info_t* downloads = NULL;
            int count = 0;

            if (scan_directory_for_downloads(download_monitors[i].watch_directory, &downloads, &count) == CDP_FILE_SUCCESS) {
                for (int j = 0; j < count; j++) {
                    /* Simple pattern matching (contains substring) */
                    if (strstr(downloads[j].filename, filename_pattern) != NULL) {
                        if (downloads[j].status == CDP_DOWNLOAD_STATUS_COMPLETED) {
                            *info = downloads[j];
                            free(downloads);
                            pthread_mutex_unlock(&filesystem_mutex);
                            return CDP_FILE_SUCCESS;
                        }
                    }
                }

                if (downloads) {
                    free(downloads);
                }
            }
        }

        pthread_mutex_unlock(&filesystem_mutex);

        /* Sleep for 100ms before checking again */
        usleep(100000);
    }

    return CDP_FILE_ERR_TIMEOUT;
}

/* Validate file exists */
int cdp_validate_file_exists(const char* file_path) {
    if (!file_path) {
        return CDP_FILE_ERR_INVALID_PATH;
    }

    struct stat st;
    if (stat(file_path, &st) == 0 && S_ISREG(st.st_mode)) {
        return CDP_FILE_SUCCESS;
    }

    return CDP_FILE_ERR_NOT_FOUND;
}

/* Get file size */
int cdp_get_file_size(const char* file_path, size_t* size) {
    if (!file_path || !size) {
        return CDP_FILE_ERR_INVALID_PATH;
    }

    struct stat st;
    if (stat(file_path, &st) == 0 && S_ISREG(st.st_mode)) {
        *size = st.st_size;
        return CDP_FILE_SUCCESS;
    }

    return CDP_FILE_ERR_NOT_FOUND;
}

/* Get file MIME type */
int cdp_get_file_mime_type(const char* file_path, char* mime_type, size_t size) {
    if (!file_path || !mime_type || size == 0) {
        return CDP_FILE_ERR_INVALID_PATH;
    }

    /* Simple MIME type detection based on file extension */
    const char* ext = strrchr(file_path, '.');
    if (!ext) {
        strncpy(mime_type, "application/octet-stream", size - 1);
        mime_type[size - 1] = '\0';
        return CDP_FILE_SUCCESS;
    }

    ext++;  /* Skip the dot */

    /* Common MIME types */
    if (strcasecmp(ext, "txt") == 0) {
        strncpy(mime_type, "text/plain", size - 1);
    } else if (strcasecmp(ext, "html") == 0 || strcasecmp(ext, "htm") == 0) {
        strncpy(mime_type, "text/html", size - 1);
    } else if (strcasecmp(ext, "css") == 0) {
        strncpy(mime_type, "text/css", size - 1);
    } else if (strcasecmp(ext, "js") == 0) {
        strncpy(mime_type, "application/javascript", size - 1);
    } else if (strcasecmp(ext, "json") == 0) {
        strncpy(mime_type, "application/json", size - 1);
    } else if (strcasecmp(ext, "xml") == 0) {
        strncpy(mime_type, "application/xml", size - 1);
    } else if (strcasecmp(ext, "pdf") == 0) {
        strncpy(mime_type, "application/pdf", size - 1);
    } else if (strcasecmp(ext, "jpg") == 0 || strcasecmp(ext, "jpeg") == 0) {
        strncpy(mime_type, "image/jpeg", size - 1);
    } else if (strcasecmp(ext, "png") == 0) {
        strncpy(mime_type, "image/png", size - 1);
    } else if (strcasecmp(ext, "gif") == 0) {
        strncpy(mime_type, "image/gif", size - 1);
    } else if (strcasecmp(ext, "svg") == 0) {
        strncpy(mime_type, "image/svg+xml", size - 1);
    } else if (strcasecmp(ext, "mp4") == 0) {
        strncpy(mime_type, "video/mp4", size - 1);
    } else if (strcasecmp(ext, "mp3") == 0) {
        strncpy(mime_type, "audio/mpeg", size - 1);
    } else if (strcasecmp(ext, "zip") == 0) {
        strncpy(mime_type, "application/zip", size - 1);
    } else {
        strncpy(mime_type, "application/octet-stream", size - 1);
    }

    mime_type[size - 1] = '\0';
    return CDP_FILE_SUCCESS;
}

/* Create directory */
int cdp_create_directory(const char* dir_path) {
    if (!dir_path) {
        return CDP_FILE_ERR_INVALID_PATH;
    }

    /* Create directory recursively */
    char path_copy[CDP_MAX_FILEPATH_LENGTH];
    strncpy(path_copy, dir_path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    char* p = path_copy;
    while (*p) {
        if (*p == '/' && p != path_copy) {
            *p = '\0';
            if (mkdir(path_copy, 0755) != 0 && errno != EEXIST) {
                return CDP_FILE_ERR_PERMISSION_DENIED;
            }
            *p = '/';
        }
        p++;
    }

    if (mkdir(path_copy, 0755) != 0 && errno != EEXIST) {
        return CDP_FILE_ERR_PERMISSION_DENIED;
    }

    return CDP_FILE_SUCCESS;
}

/* Move file */
int cdp_move_file(const char* src_path, const char* dst_path) {
    if (!src_path || !dst_path) {
        return CDP_FILE_ERR_INVALID_PATH;
    }

    if (rename(src_path, dst_path) == 0) {
        return CDP_FILE_SUCCESS;
    }

    /* If rename failed, try copy and delete */
    if (cdp_copy_file(src_path, dst_path) == CDP_FILE_SUCCESS) {
        if (unlink(src_path) == 0) {
            return CDP_FILE_SUCCESS;
        }
    }

    return CDP_FILE_ERR_PERMISSION_DENIED;
}

/* Copy file */
int cdp_copy_file(const char* src_path, const char* dst_path) {
    if (!src_path || !dst_path) {
        return CDP_FILE_ERR_INVALID_PATH;
    }

    int src_fd = open(src_path, O_RDONLY);
    if (src_fd < 0) {
        return CDP_FILE_ERR_NOT_FOUND;
    }

    int dst_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd < 0) {
        close(src_fd);
        return CDP_FILE_ERR_PERMISSION_DENIED;
    }

    char buffer[8192];
    ssize_t bytes_read, bytes_written;
    int result = CDP_FILE_SUCCESS;

    while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
        bytes_written = write(dst_fd, buffer, bytes_read);
        if (bytes_written != bytes_read) {
            result = CDP_FILE_ERR_DISK_FULL;
            break;
        }
    }

    close(src_fd);
    close(dst_fd);

    if (bytes_read < 0) {
        result = CDP_FILE_ERR_PERMISSION_DENIED;
    }

    return result;
}

/* Delete file */
int cdp_delete_file(const char* file_path) {
    if (!file_path) {
        return CDP_FILE_ERR_INVALID_PATH;
    }

    if (unlink(file_path) == 0) {
        return CDP_FILE_SUCCESS;
    }

    return CDP_FILE_ERR_PERMISSION_DENIED;
}

/* Get download list */
int cdp_get_download_list(cdp_download_info_t** downloads, int* count) {
    if (!downloads || !count) {
        return CDP_FILE_ERR_INVALID_PATH;
    }

    if (!filesystem_initialized) {
        *downloads = NULL;
        *count = 0;
        return CDP_FILE_SUCCESS;
    }

    pthread_mutex_lock(&filesystem_mutex);

    /* Count total downloads from all monitors */
    int total_downloads = 0;
    cdp_download_info_t* all_downloads[CDP_MAX_DOWNLOAD_MONITORS];
    int download_counts[CDP_MAX_DOWNLOAD_MONITORS];

    for (int i = 0; i < CDP_MAX_DOWNLOAD_MONITORS; i++) {
        all_downloads[i] = NULL;
        download_counts[i] = 0;

        if (download_monitors[i].active) {
            scan_directory_for_downloads(download_monitors[i].watch_directory,
                                        &all_downloads[i], &download_counts[i]);
            total_downloads += download_counts[i];
        }
    }

    if (total_downloads == 0) {
        *downloads = NULL;
        *count = 0;
        pthread_mutex_unlock(&filesystem_mutex);
        return CDP_FILE_SUCCESS;
    }

    /* Allocate combined list */
    cdp_download_info_t* combined_list = malloc(total_downloads * sizeof(cdp_download_info_t));
    if (!combined_list) {
        /* Cleanup allocated memory */
        for (int i = 0; i < CDP_MAX_DOWNLOAD_MONITORS; i++) {
            if (all_downloads[i]) {
                free(all_downloads[i]);
            }
        }
        pthread_mutex_unlock(&filesystem_mutex);
        return CDP_FILE_ERR_MEMORY;
    }

    /* Copy all downloads to combined list */
    int index = 0;
    for (int i = 0; i < CDP_MAX_DOWNLOAD_MONITORS; i++) {
        if (all_downloads[i]) {
            memcpy(&combined_list[index], all_downloads[i],
                   download_counts[i] * sizeof(cdp_download_info_t));
            index += download_counts[i];
            free(all_downloads[i]);
        }
    }

    *downloads = combined_list;
    *count = total_downloads;

    pthread_mutex_unlock(&filesystem_mutex);

    return CDP_FILE_SUCCESS;
}

/* Get file statistics */
int cdp_get_file_stats(cdp_file_stats_t* stats) {
    if (!stats) {
        return CDP_FILE_ERR_INVALID_PATH;
    }

    pthread_mutex_lock(&filesystem_mutex);
    *stats = g_file_stats;
    pthread_mutex_unlock(&filesystem_mutex);

    return CDP_FILE_SUCCESS;
}

/* Reset file statistics */
int cdp_reset_file_stats(void) {
    pthread_mutex_lock(&filesystem_mutex);
    memset(&g_file_stats, 0, sizeof(g_file_stats));
    g_file_stats.start_time = time(NULL);
    pthread_mutex_unlock(&filesystem_mutex);

    return CDP_FILE_SUCCESS;
}

/* Get download status string */
const char* cdp_download_status_to_string(cdp_download_status_t status) {
    switch (status) {
        case CDP_DOWNLOAD_STATUS_UNKNOWN: return "unknown";
        case CDP_DOWNLOAD_STATUS_STARTING: return "starting";
        case CDP_DOWNLOAD_STATUS_DOWNLOADING: return "downloading";
        case CDP_DOWNLOAD_STATUS_COMPLETED: return "completed";
        case CDP_DOWNLOAD_STATUS_FAILED: return "failed";
        case CDP_DOWNLOAD_STATUS_CANCELLED: return "cancelled";
        default: return "invalid";
    }
}

/* Get file error string */
const char* cdp_file_error_to_string(cdp_file_error_t error) {
    switch (error) {
        case CDP_FILE_SUCCESS: return "success";
        case CDP_FILE_ERR_NOT_FOUND: return "file not found";
        case CDP_FILE_ERR_PERMISSION_DENIED: return "permission denied";
        case CDP_FILE_ERR_INVALID_PATH: return "invalid path";
        case CDP_FILE_ERR_DISK_FULL: return "disk full";
        case CDP_FILE_ERR_FILE_TOO_LARGE: return "file too large";
        case CDP_FILE_ERR_TIMEOUT: return "timeout";
        case CDP_FILE_ERR_INVALID_FORMAT: return "invalid format";
        case CDP_FILE_ERR_NETWORK_ERROR: return "network error";
        case CDP_FILE_ERR_MONITOR_FAILED: return "monitor failed";
        case CDP_FILE_ERR_UPLOAD_FAILED: return "upload failed";
        case CDP_FILE_ERR_MEMORY: return "memory error";
        default: return "unknown error";
    }
}

/* Validate filename */
int cdp_is_valid_filename(const char* filename) {
    if (!filename || strlen(filename) == 0) {
        return 0;
    }

    /* Check for invalid characters */
    const char* invalid_chars = "<>:\"/\\|?*";
    for (const char* p = filename; *p; p++) {
        if (strchr(invalid_chars, *p) != NULL) {
            return 0;
        }
        /* Check for control characters */
        if (*p < 32) {
            return 0;
        }
    }

    /* Check for reserved names on Windows */
    const char* reserved[] = {"CON", "PRN", "AUX", "NUL",
                             "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
                             "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"};

    for (int i = 0; i < sizeof(reserved) / sizeof(reserved[0]); i++) {
        if (strcasecmp(filename, reserved[i]) == 0) {
            return 0;
        }
    }

    return 1;
}

/* Sanitize filename */
int cdp_sanitize_filename(const char* input, char* output, size_t output_size) {
    if (!input || !output || output_size == 0) {
        return CDP_FILE_ERR_INVALID_PATH;
    }

    size_t input_len = strlen(input);
    size_t output_index = 0;

    for (size_t i = 0; i < input_len && output_index < output_size - 1; i++) {
        char c = input[i];

        /* Replace invalid characters with underscore */
        if (strchr("<>:\"/\\|?*", c) != NULL || c < 32) {
            output[output_index++] = '_';
        } else {
            output[output_index++] = c;
        }
    }

    output[output_index] = '\0';

    /* Check if result is valid */
    if (!cdp_is_valid_filename(output)) {
        strncpy(output, "sanitized_file", output_size - 1);
        output[output_size - 1] = '\0';
    }

    return CDP_FILE_SUCCESS;
}

/* Get temp directory */
int cdp_get_temp_directory(char* temp_dir, size_t size) {
    if (!temp_dir || size == 0) {
        return CDP_FILE_ERR_INVALID_PATH;
    }

    strncpy(temp_dir, temp_directory, size - 1);
    temp_dir[size - 1] = '\0';

    return CDP_FILE_SUCCESS;
}

/* Get downloads directory */
int cdp_get_downloads_directory(char* downloads_dir, size_t size) {
    if (!downloads_dir || size == 0) {
        return CDP_FILE_ERR_INVALID_PATH;
    }

    /* Try to get user's Downloads directory */
    const char* home = getenv("HOME");
    if (home) {
        snprintf(downloads_dir, size, "%s/Downloads", home);

        /* Check if directory exists */
        struct stat st;
        if (stat(downloads_dir, &st) == 0 && S_ISDIR(st.st_mode)) {
            return CDP_FILE_SUCCESS;
        }
    }

    /* Fallback to temp directory */
    return cdp_get_temp_directory(downloads_dir, size);
}
