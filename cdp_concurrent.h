/**
 * CDP Concurrent Operations Framework
 * High-performance batch processing and parallel Chrome instance management
 * 
 * This module provides:
 * - Task scheduling and queue management
 * - Chrome instance pool with load balancing
 * - Fault tolerance and automatic failover
 * - Performance monitoring and metrics
 * 
 * Author: AI Assistant
 * Date: 2025-09-10
 * Task: CDP_TASK_250910A - Task 4: Batch Concurrent Operations Framework
 */

#ifndef CDP_CONCURRENT_H
#define CDP_CONCURRENT_H

#include <time.h>
#include <pthread.h>
#include "cdp_process.h"

/* Constants */
#define CDP_MAX_CONCURRENT_TASKS 100
#define CDP_MAX_TASK_TYPE_LENGTH 32
#define CDP_MAX_TASK_DATA_LENGTH 1024
#define CDP_MAX_RESULT_DATA_LENGTH 2048
#define CDP_MAX_ERROR_MESSAGE_LENGTH 512
#define CDP_DEFAULT_POOL_SIZE 5
#define CDP_MAX_POOL_SIZE 20
#define CDP_TASK_TIMEOUT_DEFAULT 60000  /* 60 seconds */
#define CDP_MAX_RETRY_ATTEMPTS 3

/* Task status enumeration */
typedef enum {
    CDP_TASK_PENDING = 0,
    CDP_TASK_QUEUED,
    CDP_TASK_RUNNING,
    CDP_TASK_COMPLETED,
    CDP_TASK_FAILED,
    CDP_TASK_CANCELLED,
    CDP_TASK_RETRYING
} cdp_task_status_t;

/* Task priority levels */
typedef enum {
    CDP_PRIORITY_LOW = 0,
    CDP_PRIORITY_NORMAL = 1,
    CDP_PRIORITY_HIGH = 2,
    CDP_PRIORITY_CRITICAL = 3
} cdp_task_priority_t;

/* Load balancing strategies */
typedef enum {
    CDP_BALANCE_ROUND_ROBIN = 0,
    CDP_BALANCE_LEAST_LOADED,
    CDP_BALANCE_PERFORMANCE,
    CDP_BALANCE_RANDOM
} cdp_balance_strategy_t;

/* Error codes for concurrent operations */
typedef enum {
    CDP_CONCURRENT_SUCCESS = 0,
    CDP_CONCURRENT_ERR_INVALID_PARAM = -4000,
    CDP_CONCURRENT_ERR_QUEUE_FULL = -4001,
    CDP_CONCURRENT_ERR_TASK_NOT_FOUND = -4002,
    CDP_CONCURRENT_ERR_POOL_EMPTY = -4003,
    CDP_CONCURRENT_ERR_INSTANCE_BUSY = -4004,
    CDP_CONCURRENT_ERR_TIMEOUT = -4005,
    CDP_CONCURRENT_ERR_CANCELLED = -4006,
    CDP_CONCURRENT_ERR_MAX_RETRIES = -4007,
    CDP_CONCURRENT_ERR_POOL_INIT_FAILED = -4008,
    CDP_CONCURRENT_ERR_MEMORY = -4009
} cdp_concurrent_error_t;

/* Forward declaration */
typedef struct cdp_task cdp_task_t;

/* Task structure */
struct cdp_task {
    /* Task identification */
    int task_id;
    char task_type[CDP_MAX_TASK_TYPE_LENGTH];
    cdp_task_priority_t priority;
    
    /* Task data */
    char task_data[CDP_MAX_TASK_DATA_LENGTH];  /* JSON format */
    char result_data[CDP_MAX_RESULT_DATA_LENGTH];
    
    /* Status tracking */
    cdp_task_status_t status;
    int assigned_instance_id;
    int retry_count;
    
    /* Timing information */
    time_t created_time;
    time_t queued_time;
    time_t started_time;
    time_t completed_time;
    int timeout_ms;
    
    /* Error handling */
    char error_message[CDP_MAX_ERROR_MESSAGE_LENGTH];
    int error_code;
    
    /* Callback */
    void (*completion_callback)(cdp_task_t* task, void* user_data);
    void* callback_data;
    
    /* Internal use */
    pthread_mutex_t mutex;
    cdp_task_t* next;
};

/* Chrome instance pool entry */
typedef struct {
    /* Instance identification */
    int instance_id;
    pid_t chrome_pid;
    int debug_port;
    int ws_socket;
    
    /* Status tracking */
    int is_available;
    int is_healthy;
    cdp_task_t* current_task;
    time_t last_used;
    
    /* Performance metrics */
    double cpu_usage;
    size_t memory_usage_mb;
    double avg_response_time_ms;
    int tasks_completed;
    int tasks_failed;
    int error_count;
    
    /* Configuration */
    cdp_chrome_config_t config;
    
    /* Thread safety */
    pthread_mutex_t mutex;
} cdp_instance_pool_entry_t;

/* Instance metrics for load balancing */
typedef struct {
    double avg_response_time;
    int success_rate;
    int current_load;
    int max_load;
    double cpu_usage;
    size_t memory_usage_mb;
} cdp_instance_metrics_t;

/* Task queue structure */
typedef struct {
    cdp_task_t* head;
    cdp_task_t* tail;
    int count;
    int max_size;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} cdp_task_queue_t;

/* Instance pool structure */
typedef struct {
    cdp_instance_pool_entry_t* instances;
    int pool_size;
    int max_size;
    int active_count;
    cdp_balance_strategy_t balance_strategy;
    pthread_mutex_t mutex;
    pthread_cond_t instance_available;
    
    /* Pool configuration */
    int auto_scale_enabled;
    int min_instances;
    int max_instances;
    double scale_up_threshold;   /* CPU/memory threshold */
    double scale_down_threshold;
    
    /* Statistics */
    int total_tasks_processed;
    int total_tasks_failed;
    time_t pool_created_time;
} cdp_instance_pool_t;

/* Performance metrics structure */
typedef struct {
    /* Task metrics */
    int total_tasks;
    int completed_tasks;
    int failed_tasks;
    int cancelled_tasks;
    int pending_tasks;
    int running_tasks;
    
    /* Performance metrics */
    double avg_task_duration_ms;
    double min_task_duration_ms;
    double max_task_duration_ms;
    double throughput_per_minute;
    
    /* Resource metrics */
    int active_instances;
    int idle_instances;
    size_t total_memory_usage_mb;
    double avg_cpu_usage;
    
    /* Timing */
    time_t start_time;
    time_t last_update;
} cdp_performance_metrics_t;

/* Batch operation result */
typedef struct {
    int task_id;
    int success;
    char result[CDP_MAX_RESULT_DATA_LENGTH];
    char error[CDP_MAX_ERROR_MESSAGE_LENGTH];
    double execution_time_ms;
} cdp_batch_result_t;

/* Task scheduler structure */
typedef struct {
    cdp_task_queue_t* queue;
    cdp_instance_pool_t* pool;
    pthread_t* worker_threads;
    int num_workers;
    int running;
    pthread_mutex_t mutex;
    
    /* Scheduler configuration */
    int max_concurrent_tasks;
    int enable_auto_retry;
    int max_retry_attempts;
    int retry_delay_ms;
    
    /* Failover configuration */
    int enable_failover;
    void (*failover_callback)(int instance_id, int task_id);
    
    /* Statistics */
    cdp_performance_metrics_t metrics;
} cdp_task_scheduler_t;

/* Callback function types */
typedef void (*cdp_task_callback_t)(cdp_task_t* task, void* user_data);
typedef void (*cdp_failover_callback_t)(int failed_instance_id, int task_id);
typedef int (*cdp_task_handler_t)(cdp_task_t* task, cdp_instance_pool_entry_t* instance);

/* Core API Functions */

/* Task Queue Management */
int cdp_create_task_queue(int max_size, cdp_task_queue_t** queue);
int cdp_destroy_task_queue(cdp_task_queue_t* queue);
int cdp_enqueue_task(cdp_task_queue_t* queue, cdp_task_t* task);
int cdp_dequeue_task(cdp_task_queue_t* queue, cdp_task_t** task);
int cdp_peek_task(cdp_task_queue_t* queue, cdp_task_t** task);
int cdp_get_queue_size(cdp_task_queue_t* queue);

/* Task Management */
int cdp_create_task(const char* task_type, const char* task_data, cdp_task_t** task);
int cdp_add_task(const char* task_type, const char* task_data, int* task_id);
int cdp_add_task_with_priority(const char* task_type, const char* task_data, cdp_task_priority_t priority, int* task_id);
int cdp_cancel_task(int task_id);
int cdp_get_task_status(int task_id, cdp_task_t* task_info);
int cdp_wait_for_task(int task_id, int timeout_ms);
int cdp_wait_for_all_tasks(int timeout_ms);
int cdp_set_task_callback(int task_id, cdp_task_callback_t callback, void* user_data);

/* Chrome Instance Pool Management */
int cdp_create_instance_pool(int initial_size, int max_size, cdp_instance_pool_t** pool);
int cdp_destroy_instance_pool(cdp_instance_pool_t* pool);
int cdp_get_available_instance(cdp_instance_pool_t* pool, cdp_instance_pool_entry_t** instance);
int cdp_return_instance_to_pool(cdp_instance_pool_t* pool, int instance_id);
int cdp_scale_instance_pool(cdp_instance_pool_t* pool, int target_size);
int cdp_cleanup_instance_pool(cdp_instance_pool_t* pool);
int cdp_set_pool_auto_scale(cdp_instance_pool_t* pool, int enable, int min, int max);

/* Load Balancing and Failover */
int cdp_get_pool_instance_metrics(int instance_id, cdp_instance_metrics_t* metrics);
int cdp_set_load_balancing_strategy(cdp_instance_pool_t* pool, cdp_balance_strategy_t strategy);
int cdp_enable_auto_failover(cdp_task_scheduler_t* scheduler, int enable);
int cdp_set_failover_callback(cdp_task_scheduler_t* scheduler, cdp_failover_callback_t callback);

/* Task Scheduler */
int cdp_create_task_scheduler(int max_concurrent, cdp_task_scheduler_t** scheduler);
int cdp_destroy_task_scheduler(cdp_task_scheduler_t* scheduler);
int cdp_start_scheduler(cdp_task_scheduler_t* scheduler);
int cdp_stop_scheduler(cdp_task_scheduler_t* scheduler);
int cdp_submit_task(cdp_task_scheduler_t* scheduler, cdp_task_t* task);
int cdp_register_task_handler(const char* task_type, cdp_task_handler_t handler);

/* Batch Operations */
int cdp_batch_url_check(const char** urls, int count, int max_concurrent, char* results_json);
int cdp_batch_screenshot_urls(const char** urls, int count, const char* output_dir, int max_concurrent);
int cdp_batch_performance_test(const char** urls, int count, int duration_sec, const char* output_file);
int cdp_batch_form_submission(const char* form_config_json, int max_concurrent);
int cdp_batch_execute_script(const char* script, const char** urls, int count, cdp_batch_result_t* results);

/* Load Testing */
int cdp_load_test_single_url(const char* url, int concurrent_users, int duration_sec, const char* output_file);
int cdp_stress_test_scenario(const char* scenario_json, const char* output_file);
int cdp_benchmark_chrome_instances(int num_instances, const char* test_url, cdp_performance_metrics_t* metrics);

/* Monitoring and Metrics */
int cdp_get_performance_metrics(cdp_task_scheduler_t* scheduler, cdp_performance_metrics_t* metrics);
int cdp_start_performance_monitoring(cdp_task_scheduler_t* scheduler, const char* output_file);
int cdp_stop_performance_monitoring(cdp_task_scheduler_t* scheduler);
int cdp_export_performance_report(cdp_task_scheduler_t* scheduler, const char* format, const char* output_file);
int cdp_get_instance_pool_stats(cdp_instance_pool_t* pool, int* active, int* idle, int* failed);

/* Utility Functions */
const char* cdp_task_status_to_string(cdp_task_status_t status);
const char* cdp_task_priority_to_string(cdp_task_priority_t priority);
const char* cdp_balance_strategy_to_string(cdp_balance_strategy_t strategy);
const char* cdp_concurrent_error_to_string(cdp_concurrent_error_t error);
int cdp_set_task_timeout_default(int timeout_ms);
int cdp_set_max_retry_attempts(int max_retries);

/* Configuration */
int cdp_configure_scheduler(cdp_task_scheduler_t* scheduler, const char* config_json);
int cdp_set_instance_pool_config(cdp_instance_pool_t* pool, const cdp_chrome_config_t* config);

/* Initialization and Cleanup */
int cdp_init_concurrent_module(void);
int cdp_cleanup_concurrent_module(void);

/* Global scheduler instance (optional) */
extern cdp_task_scheduler_t* g_default_scheduler;

#endif /* CDP_CONCURRENT_H */