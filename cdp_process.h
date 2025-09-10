/**
 * CDP Process Management Module
 * Enhanced Chrome process lifecycle management with multi-instance support
 * 
 * This module provides system-level Chrome process management capabilities
 * that Chrome cannot perform on itself due to security sandbox limitations.
 * 
 * Features:
 * - Multi-instance Chrome management
 * - Process health monitoring and auto-recovery
 * - Flexible configuration and parameter management
 * - Resource cleanup and emergency recovery
 * 
 * Author: AI Assistant
 * Date: 2025-09-10
 * Task: CDP_TASK_250910A - Task 1: Chrome Process Management Enhancement
 */

#ifndef CDP_PROCESS_H
#define CDP_PROCESS_H

#include <sys/types.h>
#include <time.h>
#include <pthread.h>

/* Constants */
#define CDP_MAX_CHROME_INSTANCES 32
#define CDP_MAX_PATH_LENGTH 512
#define CDP_MAX_FLAGS_LENGTH 1024
#define CDP_MAX_STATUS_LENGTH 32
#define CDP_MAX_ERROR_MESSAGE 256
#define CDP_DEFAULT_HEALTH_CHECK_INTERVAL 30
#define CDP_MAX_RESTART_ATTEMPTS 3
#define CDP_PROCESS_TIMEOUT_SEC 30

/* Chrome instance status */
typedef enum {
    CDP_CHROME_STATUS_UNKNOWN = 0,
    CDP_CHROME_STATUS_STARTING,
    CDP_CHROME_STATUS_RUNNING,
    CDP_CHROME_STATUS_STOPPING,
    CDP_CHROME_STATUS_STOPPED,
    CDP_CHROME_STATUS_CRASHED,
    CDP_CHROME_STATUS_FAILED
} cdp_chrome_status_t;

/* Chrome configuration structure */
typedef struct {
    /* Basic configuration */
    char profile_dir[CDP_MAX_PATH_LENGTH];
    char user_data_dir[CDP_MAX_PATH_LENGTH];
    char extra_flags[CDP_MAX_FLAGS_LENGTH];
    int debug_port;
    int window_width;
    int window_height;
    
    /* Behavior flags */
    int headless;
    int incognito;
    int disable_gpu;
    int no_sandbox;
    int disable_dev_shm_usage;
    
    /* Network configuration */
    char proxy_server[256];
    char user_agent[512];
    
    /* Advanced options */
    int memory_limit_mb;
    int timeout_sec;
    int auto_restart;
    int max_restart_attempts;
    
    /* Internal flags */
    int config_validated;
    time_t created_time;
} cdp_chrome_config_t;

/* Chrome instance information */
typedef struct {
    /* Instance identification */
    int instance_id;
    pid_t pid;
    int debug_port;
    
    /* Paths and configuration */
    char profile_path[CDP_MAX_PATH_LENGTH];
    char user_data_dir[CDP_MAX_PATH_LENGTH];
    cdp_chrome_config_t config;
    
    /* Status and timing */
    cdp_chrome_status_t status;
    time_t start_time;
    time_t last_health_check;
    time_t last_activity;
    
    /* Health monitoring */
    int health_check_failures;
    int restart_count;
    int auto_restart_enabled;
    
    /* Resource usage */
    double cpu_usage;
    size_t memory_usage_mb;
    
    /* Error tracking */
    char last_error[CDP_MAX_ERROR_MESSAGE];
    int error_count;
    
    /* Thread safety */
    pthread_mutex_t mutex;
    
    /* Internal state */
    int websocket_connected;
    time_t last_websocket_activity;
} cdp_chrome_instance_t;

/* Chrome instance registry */
typedef struct {
    cdp_chrome_instance_t instances[CDP_MAX_CHROME_INSTANCES];
    int instance_count;
    int next_instance_id;
    int next_debug_port;
    pthread_mutex_t registry_mutex;
    
    /* Global settings */
    int health_check_interval;
    int auto_cleanup_enabled;
    char temp_dir[CDP_MAX_PATH_LENGTH];
} cdp_chrome_registry_t;

/* Health check callback function type */
typedef void (*cdp_health_callback_t)(cdp_chrome_instance_t* instance, void* user_data);

/* Error codes specific to process management */
typedef enum {
    CDP_PROCESS_SUCCESS = 0,
    CDP_PROCESS_ERR_INVALID_CONFIG = -1000,
    CDP_PROCESS_ERR_LAUNCH_FAILED = -1001,
    CDP_PROCESS_ERR_INSTANCE_NOT_FOUND = -1002,
    CDP_PROCESS_ERR_INSTANCE_LIMIT_REACHED = -1003,
    CDP_PROCESS_ERR_KILL_FAILED = -1004,
    CDP_PROCESS_ERR_HEALTH_CHECK_FAILED = -1005,
    CDP_PROCESS_ERR_CLEANUP_FAILED = -1006,
    CDP_PROCESS_ERR_PORT_CONFLICT = -1007,
    CDP_PROCESS_ERR_PERMISSION_DENIED = -1008,
    CDP_PROCESS_ERR_TIMEOUT = -1009,
    CDP_PROCESS_ERR_MEMORY = -1010
} cdp_process_error_t;

/* Core API Functions */

/* Configuration Management */
int cdp_create_chrome_config(cdp_chrome_config_t* config);
int cdp_validate_chrome_config(const cdp_chrome_config_t* config);
int cdp_apply_chrome_config(const cdp_chrome_config_t* config, int instance_id);
int cdp_copy_chrome_config(const cdp_chrome_config_t* src, cdp_chrome_config_t* dst);

/* Instance Management */
int cdp_launch_chrome_instance(const cdp_chrome_config_t* config, cdp_chrome_instance_t* instance);
int cdp_kill_chrome_instance(int instance_id, int force);
int cdp_list_chrome_instances(cdp_chrome_instance_t** instances, int* count);
int cdp_get_instance_status(int instance_id, cdp_chrome_instance_t* status);
int cdp_find_instance_by_pid(pid_t pid, cdp_chrome_instance_t** instance);
int cdp_find_instance_by_port(int debug_port, cdp_chrome_instance_t** instance);

/* Health Monitoring */
int cdp_monitor_chrome_health(int instance_id, int check_interval_sec);
int cdp_set_health_check_callback(cdp_health_callback_t callback, void* user_data);
int cdp_auto_restart_on_crash(int instance_id, int max_restart_attempts);
int cdp_check_instance_health(int instance_id);
int cdp_get_instance_metrics(int instance_id, double* cpu_usage, size_t* memory_mb);

/* Recovery and Cleanup */
int cdp_clean_restart_chrome(int instance_id, const char* reason);
int cdp_emergency_cleanup_chrome_processes(void);
int cdp_cleanup_instance_resources(int instance_id);
int cdp_cleanup_temp_files(const char* temp_dir);

/* Registry Management */
int cdp_init_chrome_registry(void);
int cdp_cleanup_chrome_registry(void);
int cdp_get_registry_stats(int* total_instances, int* running_instances, int* failed_instances);

/* Utility Functions */
int cdp_is_chrome_running(pid_t pid);
int cdp_get_available_debug_port(int start_port);
int cdp_wait_for_chrome_ready(int instance_id, int timeout_sec);
const char* cdp_chrome_status_to_string(cdp_chrome_status_t status);
const char* cdp_process_error_to_string(cdp_process_error_t error);

/* Global registry instance */
extern cdp_chrome_registry_t g_chrome_registry;

#endif /* CDP_PROCESS_H */
