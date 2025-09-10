/**
 * CDP Concurrent Operations Framework Implementation
 * High-performance batch processing and parallel Chrome instance management
 * 
 * Author: AI Assistant
 * Date: 2025-09-10  
 * Task: CDP_TASK_250910A - Task 4: Batch Concurrent Operations Framework
 */

#include "cdp_concurrent.h"
#include "cdp_process.h"
#include "cdp_filesystem.h"
#include "cdp_system.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <signal.h>
#include <limits.h>
#include <float.h>

/* Global default scheduler */
cdp_task_scheduler_t* g_default_scheduler = NULL;

/* Task ID counter */
static int g_next_task_id = 1;
static pthread_mutex_t g_task_id_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Task registry for tracking all tasks */
typedef struct task_registry_node {
    cdp_task_t* task;
    struct task_registry_node* next;
} task_registry_node_t;

static task_registry_node_t* g_task_registry = NULL;
static pthread_mutex_t g_registry_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Task handlers registry */
typedef struct task_handler_node {
    char task_type[CDP_MAX_TASK_TYPE_LENGTH];
    cdp_task_handler_t handler;
    struct task_handler_node* next;
} task_handler_node_t;

static task_handler_node_t* g_task_handlers = NULL;
static pthread_mutex_t g_handlers_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Forward declarations */
static void* scheduler_worker_thread(void* arg);
static int execute_task_on_instance(cdp_task_t* task, cdp_instance_pool_entry_t* instance);
static int handle_task_completion(cdp_task_t* task, int success, const char* result);
static int select_instance_by_strategy(cdp_instance_pool_t* pool, cdp_instance_pool_entry_t** instance);
static void update_instance_metrics(cdp_instance_pool_entry_t* instance, double response_time, int success);

/* Initialize concurrent module */
int cdp_init_concurrent_module(void) {
    /* Initialize task registry */
    g_task_registry = NULL;
    g_next_task_id = 1;
    
    /* Initialize default configurations */
    return CDP_CONCURRENT_SUCCESS;
}

/* Cleanup concurrent module */
int cdp_cleanup_concurrent_module(void) {
    /* Cleanup task registry */
    pthread_mutex_lock(&g_registry_mutex);
    task_registry_node_t* node = g_task_registry;
    while (node) {
        task_registry_node_t* next = node->next;
        if (node->task) {
            pthread_mutex_destroy(&node->task->mutex);
            free(node->task);
        }
        free(node);
        node = next;
    }
    g_task_registry = NULL;
    pthread_mutex_unlock(&g_registry_mutex);
    
    /* Cleanup task handlers */
    pthread_mutex_lock(&g_handlers_mutex);
    task_handler_node_t* handler = g_task_handlers;
    while (handler) {
        task_handler_node_t* next = handler->next;
        free(handler);
        handler = next;
    }
    g_task_handlers = NULL;
    pthread_mutex_unlock(&g_handlers_mutex);
    
    /* Cleanup default scheduler if exists */
    if (g_default_scheduler) {
        cdp_destroy_task_scheduler(g_default_scheduler);
        g_default_scheduler = NULL;
    }
    
    return CDP_CONCURRENT_SUCCESS;
}

/* Create task queue */
int cdp_create_task_queue(int max_size, cdp_task_queue_t** queue) {
    if (!queue || max_size <= 0) {
        return CDP_CONCURRENT_ERR_INVALID_PARAM;
    }
    
    *queue = malloc(sizeof(cdp_task_queue_t));
    if (!*queue) {
        return CDP_CONCURRENT_ERR_MEMORY;
    }
    
    (*queue)->head = NULL;
    (*queue)->tail = NULL;
    (*queue)->count = 0;
    (*queue)->max_size = max_size;
    
    pthread_mutex_init(&(*queue)->mutex, NULL);
    pthread_cond_init(&(*queue)->not_empty, NULL);
    pthread_cond_init(&(*queue)->not_full, NULL);
    
    return CDP_CONCURRENT_SUCCESS;
}

/* Get queue size */
int cdp_get_queue_size(cdp_task_queue_t* queue) {
    if (!queue) {
        return -1;
    }
    
    pthread_mutex_lock(&queue->mutex);
    int size = queue->count;
    pthread_mutex_unlock(&queue->mutex);
    
    return size;
}

/* Destroy task queue */
int cdp_destroy_task_queue(cdp_task_queue_t* queue) {
    if (!queue) {
        return CDP_CONCURRENT_ERR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&queue->mutex);
    
    /* Free all remaining tasks */
    cdp_task_t* task = queue->head;
    while (task) {
        cdp_task_t* next = task->next;
        pthread_mutex_destroy(&task->mutex);
        free(task);
        task = next;
    }
    
    pthread_mutex_unlock(&queue->mutex);
    
    pthread_mutex_destroy(&queue->mutex);
    pthread_cond_destroy(&queue->not_empty);
    pthread_cond_destroy(&queue->not_full);
    
    free(queue);
    return CDP_CONCURRENT_SUCCESS;
}

/* Enqueue task */
int cdp_enqueue_task(cdp_task_queue_t* queue, cdp_task_t* task) {
    if (!queue || !task) {
        return CDP_CONCURRENT_ERR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&queue->mutex);
    
    /* Wait if queue is full */
    while (queue->count >= queue->max_size) {
        pthread_cond_wait(&queue->not_full, &queue->mutex);
    }
    
    /* Add task to queue */
    task->next = NULL;
    task->status = CDP_TASK_QUEUED;
    task->queued_time = time(NULL);
    
    if (queue->tail) {
        queue->tail->next = task;
    } else {
        queue->head = task;
    }
    queue->tail = task;
    queue->count++;
    
    /* Signal that queue is not empty */
    pthread_cond_signal(&queue->not_empty);
    
    pthread_mutex_unlock(&queue->mutex);
    return CDP_CONCURRENT_SUCCESS;
}

/* Dequeue task */
int cdp_dequeue_task(cdp_task_queue_t* queue, cdp_task_t** task) {
    if (!queue || !task) {
        return CDP_CONCURRENT_ERR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&queue->mutex);
    
    /* Wait if queue is empty */
    while (queue->count == 0) {
        pthread_cond_wait(&queue->not_empty, &queue->mutex);
    }
    
    /* Remove task from queue */
    *task = queue->head;
    queue->head = (*task)->next;
    if (!queue->head) {
        queue->tail = NULL;
    }
    (*task)->next = NULL;
    queue->count--;
    
    /* Signal that queue is not full */
    pthread_cond_signal(&queue->not_full);
    
    pthread_mutex_unlock(&queue->mutex);
    return CDP_CONCURRENT_SUCCESS;
}

/* Create task */
int cdp_create_task(const char* task_type, const char* task_data, cdp_task_t** task) {
    if (!task_type || !task) {
        return CDP_CONCURRENT_ERR_INVALID_PARAM;
    }
    
    *task = malloc(sizeof(cdp_task_t));
    if (!*task) {
        return CDP_CONCURRENT_ERR_MEMORY;
    }
    
    memset(*task, 0, sizeof(cdp_task_t));
    
    /* Generate unique task ID */
    pthread_mutex_lock(&g_task_id_mutex);
    (*task)->task_id = g_next_task_id++;
    pthread_mutex_unlock(&g_task_id_mutex);
    
    /* Set task properties */
    strncpy((*task)->task_type, task_type, CDP_MAX_TASK_TYPE_LENGTH - 1);
    if (task_data) {
        strncpy((*task)->task_data, task_data, CDP_MAX_TASK_DATA_LENGTH - 1);
    }
    
    (*task)->status = CDP_TASK_PENDING;
    (*task)->priority = CDP_PRIORITY_NORMAL;
    (*task)->created_time = time(NULL);
    (*task)->timeout_ms = CDP_TASK_TIMEOUT_DEFAULT;
    (*task)->retry_count = 0;
    
    pthread_mutex_init(&(*task)->mutex, NULL);
    
    /* Add to registry */
    pthread_mutex_lock(&g_registry_mutex);
    task_registry_node_t* node = malloc(sizeof(task_registry_node_t));
    if (node) {
        node->task = *task;
        node->next = g_task_registry;
        g_task_registry = node;
    }
    pthread_mutex_unlock(&g_registry_mutex);
    
    return CDP_CONCURRENT_SUCCESS;
}

/* Add task to scheduler */
int cdp_add_task(const char* task_type, const char* task_data, int* task_id) {
    cdp_task_t* task;
    int ret = cdp_create_task(task_type, task_data, &task);
    if (ret != CDP_CONCURRENT_SUCCESS) {
        return ret;
    }
    
    if (task_id) {
        *task_id = task->task_id;
    }
    
    /* Submit to default scheduler if available */
    if (g_default_scheduler) {
        return cdp_submit_task(g_default_scheduler, task);
    }
    
    return CDP_CONCURRENT_SUCCESS;
}

/* Cancel task */
int cdp_cancel_task(int task_id) {
    pthread_mutex_lock(&g_registry_mutex);
    
    task_registry_node_t* node = g_task_registry;
    while (node) {
        if (node->task && node->task->task_id == task_id) {
            pthread_mutex_lock(&node->task->mutex);
            
            if (node->task->status == CDP_TASK_PENDING || 
                node->task->status == CDP_TASK_QUEUED) {
                node->task->status = CDP_TASK_CANCELLED;
                pthread_mutex_unlock(&node->task->mutex);
                pthread_mutex_unlock(&g_registry_mutex);
                return CDP_CONCURRENT_SUCCESS;
            }
            
            pthread_mutex_unlock(&node->task->mutex);
            pthread_mutex_unlock(&g_registry_mutex);
            return CDP_CONCURRENT_ERR_CANCELLED;
        }
        node = node->next;
    }
    
    pthread_mutex_unlock(&g_registry_mutex);
    return CDP_CONCURRENT_ERR_TASK_NOT_FOUND;
}

/* Get task status */
int cdp_get_task_status(int task_id, cdp_task_t* task_info) {
    if (!task_info) {
        return CDP_CONCURRENT_ERR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&g_registry_mutex);
    
    task_registry_node_t* node = g_task_registry;
    while (node) {
        if (node->task && node->task->task_id == task_id) {
            pthread_mutex_lock(&node->task->mutex);
            memcpy(task_info, node->task, sizeof(cdp_task_t));
            pthread_mutex_unlock(&node->task->mutex);
            pthread_mutex_unlock(&g_registry_mutex);
            return CDP_CONCURRENT_SUCCESS;
        }
        node = node->next;
    }
    
    pthread_mutex_unlock(&g_registry_mutex);
    return CDP_CONCURRENT_ERR_TASK_NOT_FOUND;
}

/* Create instance pool */
int cdp_create_instance_pool(int initial_size, int max_size, cdp_instance_pool_t** pool) {
    if (!pool || initial_size <= 0 || max_size < initial_size) {
        return CDP_CONCURRENT_ERR_INVALID_PARAM;
    }
    
    *pool = malloc(sizeof(cdp_instance_pool_t));
    if (!*pool) {
        return CDP_CONCURRENT_ERR_MEMORY;
    }
    
    memset(*pool, 0, sizeof(cdp_instance_pool_t));
    
    (*pool)->instances = calloc(max_size, sizeof(cdp_instance_pool_entry_t));
    if (!(*pool)->instances) {
        free(*pool);
        return CDP_CONCURRENT_ERR_MEMORY;
    }
    
    (*pool)->pool_size = 0;
    (*pool)->max_size = max_size;
    (*pool)->active_count = 0;
    (*pool)->balance_strategy = CDP_BALANCE_ROUND_ROBIN;
    (*pool)->pool_created_time = time(NULL);
    
    pthread_mutex_init(&(*pool)->mutex, NULL);
    pthread_cond_init(&(*pool)->instance_available, NULL);
    
    /* Initialize instances */
    for (int i = 0; i < initial_size; i++) {
        cdp_instance_pool_entry_t* entry = &(*pool)->instances[i];
        entry->instance_id = i;
        entry->is_available = 1;
        entry->is_healthy = 1;
        entry->last_used = time(NULL);
        pthread_mutex_init(&entry->mutex, NULL);
        
        /* Create Chrome configuration */
        cdp_chrome_config_t config;
        cdp_create_chrome_config(&config);
        config.debug_port = 9222 + i;
        config.headless = 1;
        
        /* Launch Chrome instance */
        cdp_chrome_instance_t chrome_instance;
        if (cdp_launch_chrome_instance(&config, &chrome_instance) == CDP_PROCESS_SUCCESS) {
            entry->chrome_pid = chrome_instance.pid;
            entry->debug_port = config.debug_port;
            memcpy(&entry->config, &config, sizeof(cdp_chrome_config_t));
            (*pool)->pool_size++;
        } else {
            entry->is_healthy = 0;
        }
    }
    
    if ((*pool)->pool_size == 0) {
        free((*pool)->instances);
        free(*pool);
        return CDP_CONCURRENT_ERR_POOL_INIT_FAILED;
    }
    
    return CDP_CONCURRENT_SUCCESS;
}

/* Destroy instance pool */
int cdp_destroy_instance_pool(cdp_instance_pool_t* pool) {
    if (!pool) {
        return CDP_CONCURRENT_ERR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&pool->mutex);
    
    /* Terminate all Chrome instances */
    for (int i = 0; i < pool->max_size; i++) {
        cdp_instance_pool_entry_t* entry = &pool->instances[i];
        if (entry->chrome_pid > 0) {
            cdp_kill_chrome_instance(entry->instance_id, 0);
        }
        pthread_mutex_destroy(&entry->mutex);
    }
    
    free(pool->instances);
    pthread_mutex_unlock(&pool->mutex);
    
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->instance_available);
    
    free(pool);
    return CDP_CONCURRENT_SUCCESS;
}

/* Get available instance from pool */
int cdp_get_available_instance(cdp_instance_pool_t* pool, cdp_instance_pool_entry_t** instance) {
    if (!pool || !instance) {
        return CDP_CONCURRENT_ERR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&pool->mutex);
    
    /* Wait for available instance */
    while (1) {
        int ret = select_instance_by_strategy(pool, instance);
        if (ret == CDP_CONCURRENT_SUCCESS) {
            (*instance)->is_available = 0;
            (*instance)->last_used = time(NULL);
            pool->active_count++;
            pthread_mutex_unlock(&pool->mutex);
            return CDP_CONCURRENT_SUCCESS;
        }
        
        /* Wait for instance to become available */
        pthread_cond_wait(&pool->instance_available, &pool->mutex);
    }
}

/* Return instance to pool */
int cdp_return_instance_to_pool(cdp_instance_pool_t* pool, int instance_id) {
    if (!pool) {
        return CDP_CONCURRENT_ERR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&pool->mutex);
    
    for (int i = 0; i < pool->max_size; i++) {
        if (pool->instances[i].instance_id == instance_id) {
            pool->instances[i].is_available = 1;
            pool->instances[i].current_task = NULL;
            pool->active_count--;
            
            /* Signal that instance is available */
            pthread_cond_signal(&pool->instance_available);
            
            pthread_mutex_unlock(&pool->mutex);
            return CDP_CONCURRENT_SUCCESS;
        }
    }
    
    pthread_mutex_unlock(&pool->mutex);
    return CDP_CONCURRENT_ERR_TASK_NOT_FOUND;
}

/* Create task scheduler */
int cdp_create_task_scheduler(int max_concurrent, cdp_task_scheduler_t** scheduler) {
    if (!scheduler || max_concurrent <= 0) {
        return CDP_CONCURRENT_ERR_INVALID_PARAM;
    }
    
    *scheduler = malloc(sizeof(cdp_task_scheduler_t));
    if (!*scheduler) {
        return CDP_CONCURRENT_ERR_MEMORY;
    }
    
    memset(*scheduler, 0, sizeof(cdp_task_scheduler_t));
    
    /* Create task queue */
    int ret = cdp_create_task_queue(CDP_MAX_CONCURRENT_TASKS, &(*scheduler)->queue);
    if (ret != CDP_CONCURRENT_SUCCESS) {
        free(*scheduler);
        return ret;
    }
    
    /* Create instance pool */
    ret = cdp_create_instance_pool(max_concurrent, CDP_MAX_POOL_SIZE, &(*scheduler)->pool);
    if (ret != CDP_CONCURRENT_SUCCESS) {
        cdp_destroy_task_queue((*scheduler)->queue);
        free(*scheduler);
        return ret;
    }
    
    (*scheduler)->max_concurrent_tasks = max_concurrent;
    (*scheduler)->num_workers = max_concurrent;
    (*scheduler)->running = 0;
    (*scheduler)->enable_auto_retry = 1;
    (*scheduler)->max_retry_attempts = CDP_MAX_RETRY_ATTEMPTS;
    (*scheduler)->retry_delay_ms = 1000;
    (*scheduler)->metrics.start_time = time(NULL);
    
    pthread_mutex_init(&(*scheduler)->mutex, NULL);
    
    /* Allocate worker threads */
    (*scheduler)->worker_threads = calloc(max_concurrent, sizeof(pthread_t));
    if (!(*scheduler)->worker_threads) {
        cdp_destroy_task_queue((*scheduler)->queue);
        cdp_destroy_instance_pool((*scheduler)->pool);
        free(*scheduler);
        return CDP_CONCURRENT_ERR_MEMORY;
    }
    
    return CDP_CONCURRENT_SUCCESS;
}

/* Destroy task scheduler */
int cdp_destroy_task_scheduler(cdp_task_scheduler_t* scheduler) {
    if (!scheduler) {
        return CDP_CONCURRENT_ERR_INVALID_PARAM;
    }
    
    /* Stop scheduler if running */
    if (scheduler->running) {
        cdp_stop_scheduler(scheduler);
    }
    
    /* Cleanup resources */
    if (scheduler->queue) {
        cdp_destroy_task_queue(scheduler->queue);
    }
    
    if (scheduler->pool) {
        cdp_destroy_instance_pool(scheduler->pool);
    }
    
    if (scheduler->worker_threads) {
        free(scheduler->worker_threads);
    }
    
    pthread_mutex_destroy(&scheduler->mutex);
    free(scheduler);
    
    return CDP_CONCURRENT_SUCCESS;
}

/* Start scheduler */
int cdp_start_scheduler(cdp_task_scheduler_t* scheduler) {
    if (!scheduler) {
        return CDP_CONCURRENT_ERR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&scheduler->mutex);
    
    if (scheduler->running) {
        pthread_mutex_unlock(&scheduler->mutex);
        return CDP_CONCURRENT_SUCCESS;
    }
    
    scheduler->running = 1;
    
    /* Start worker threads */
    for (int i = 0; i < scheduler->num_workers; i++) {
        pthread_create(&scheduler->worker_threads[i], NULL, scheduler_worker_thread, scheduler);
    }
    
    pthread_mutex_unlock(&scheduler->mutex);
    return CDP_CONCURRENT_SUCCESS;
}

/* Stop scheduler */
int cdp_stop_scheduler(cdp_task_scheduler_t* scheduler) {
    if (!scheduler) {
        return CDP_CONCURRENT_ERR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&scheduler->mutex);
    scheduler->running = 0;
    pthread_mutex_unlock(&scheduler->mutex);
    
    /* Signal all worker threads to stop */
    pthread_cond_broadcast(&scheduler->queue->not_empty);
    
    /* Wait for worker threads to finish */
    for (int i = 0; i < scheduler->num_workers; i++) {
        pthread_join(scheduler->worker_threads[i], NULL);
    }
    
    return CDP_CONCURRENT_SUCCESS;
}

/* Submit task to scheduler */
int cdp_submit_task(cdp_task_scheduler_t* scheduler, cdp_task_t* task) {
    if (!scheduler || !task) {
        return CDP_CONCURRENT_ERR_INVALID_PARAM;
    }
    
    return cdp_enqueue_task(scheduler->queue, task);
}

/* Worker thread function */
static void* scheduler_worker_thread(void* arg) {
    cdp_task_scheduler_t* scheduler = (cdp_task_scheduler_t*)arg;
    
    while (scheduler->running) {
        cdp_task_t* task = NULL;
        
        /* Get task from queue */
        if (cdp_dequeue_task(scheduler->queue, &task) != CDP_CONCURRENT_SUCCESS) {
            continue;
        }
        
        /* Get available instance */
        cdp_instance_pool_entry_t* instance = NULL;
        if (cdp_get_available_instance(scheduler->pool, &instance) != CDP_CONCURRENT_SUCCESS) {
            /* Re-queue task */
            cdp_enqueue_task(scheduler->queue, task);
            continue;
        }
        
        /* Execute task */
        pthread_mutex_lock(&task->mutex);
        task->status = CDP_TASK_RUNNING;
        task->started_time = time(NULL);
        task->assigned_instance_id = instance->instance_id;
        instance->current_task = task;
        pthread_mutex_unlock(&task->mutex);
        
        int success = execute_task_on_instance(task, instance);
        
        /* Handle completion */
        handle_task_completion(task, success, task->result_data);
        
        /* Update metrics */
        double response_time = difftime(time(NULL), task->started_time) * 1000;
        update_instance_metrics(instance, response_time, success);
        
        /* Return instance to pool */
        cdp_return_instance_to_pool(scheduler->pool, instance->instance_id);
        
        /* Update scheduler metrics */
        pthread_mutex_lock(&scheduler->mutex);
        scheduler->metrics.total_tasks++;
        if (success) {
            scheduler->metrics.completed_tasks++;
        } else {
            scheduler->metrics.failed_tasks++;
        }
        pthread_mutex_unlock(&scheduler->mutex);
    }
    
    return NULL;
}

/* Execute task on instance */
static int execute_task_on_instance(cdp_task_t* task, cdp_instance_pool_entry_t* instance) {
    /* Look for registered handler */
    pthread_mutex_lock(&g_handlers_mutex);
    task_handler_node_t* handler = g_task_handlers;
    while (handler) {
        if (strcmp(handler->task_type, task->task_type) == 0) {
            pthread_mutex_unlock(&g_handlers_mutex);
            return handler->handler(task, instance);
        }
        handler = handler->next;
    }
    pthread_mutex_unlock(&g_handlers_mutex);
    
    /* Default handling - execute as Chrome command */
    /* This is a placeholder - actual implementation would connect to Chrome DevTools */
    snprintf(task->result_data, CDP_MAX_RESULT_DATA_LENGTH,
        "{\"status\":\"executed\",\"instance\":%d,\"port\":%d}",
        instance->instance_id, instance->debug_port);
    
    return 1;  /* Success */
}

/* Handle task completion */
static int handle_task_completion(cdp_task_t* task, int success, const char* result) {
    pthread_mutex_lock(&task->mutex);
    
    if (success) {
        task->status = CDP_TASK_COMPLETED;
        task->completed_time = time(NULL);
        if (result) {
            strncpy(task->result_data, result, CDP_MAX_RESULT_DATA_LENGTH - 1);
        }
    } else {
        task->status = CDP_TASK_FAILED;
        task->completed_time = time(NULL);
    }
    
    /* Call completion callback if set */
    if (task->completion_callback) {
        task->completion_callback(task, task->callback_data);
    }
    
    pthread_mutex_unlock(&task->mutex);
    return CDP_CONCURRENT_SUCCESS;
}

/* Select instance by load balancing strategy */
static int select_instance_by_strategy(cdp_instance_pool_t* pool, cdp_instance_pool_entry_t** instance) {
    *instance = NULL;
    
    switch (pool->balance_strategy) {
        case CDP_BALANCE_ROUND_ROBIN: {
            /* Round-robin selection */
            static int next_index = 0;
            for (int i = 0; i < pool->max_size; i++) {
                int idx = (next_index + i) % pool->max_size;
                if (pool->instances[idx].is_available && pool->instances[idx].is_healthy) {
                    *instance = &pool->instances[idx];
                    next_index = (idx + 1) % pool->max_size;
                    return CDP_CONCURRENT_SUCCESS;
                }
            }
            break;
        }
        
        case CDP_BALANCE_LEAST_LOADED: {
            /* Select instance with lowest load */
            int min_load = INT_MAX;
            for (int i = 0; i < pool->max_size; i++) {
                if (pool->instances[i].is_available && pool->instances[i].is_healthy) {
                    int load = pool->instances[i].tasks_completed + pool->instances[i].tasks_failed;
                    if (load < min_load) {
                        min_load = load;
                        *instance = &pool->instances[i];
                    }
                }
            }
            break;
        }
        
        case CDP_BALANCE_PERFORMANCE: {
            /* Select instance with best performance */
            double best_time = DBL_MAX;
            for (int i = 0; i < pool->max_size; i++) {
                if (pool->instances[i].is_available && pool->instances[i].is_healthy) {
                    if (pool->instances[i].avg_response_time_ms < best_time) {
                        best_time = pool->instances[i].avg_response_time_ms;
                        *instance = &pool->instances[i];
                    }
                }
            }
            break;
        }
        
        case CDP_BALANCE_RANDOM: {
            /* Random selection */
            int available_count = 0;
            for (int i = 0; i < pool->max_size; i++) {
                if (pool->instances[i].is_available && pool->instances[i].is_healthy) {
                    available_count++;
                }
            }
            
            if (available_count > 0) {
                int target = rand() % available_count;
                int count = 0;
                for (int i = 0; i < pool->max_size; i++) {
                    if (pool->instances[i].is_available && pool->instances[i].is_healthy) {
                        if (count == target) {
                            *instance = &pool->instances[i];
                            return CDP_CONCURRENT_SUCCESS;
                        }
                        count++;
                    }
                }
            }
            break;
        }
    }
    
    return *instance ? CDP_CONCURRENT_SUCCESS : CDP_CONCURRENT_ERR_POOL_EMPTY;
}

/* Update instance metrics */
static void update_instance_metrics(cdp_instance_pool_entry_t* instance, double response_time, int success) {
    pthread_mutex_lock(&instance->mutex);
    
    if (success) {
        instance->tasks_completed++;
    } else {
        instance->tasks_failed++;
        instance->error_count++;
    }
    
    /* Update average response time */
    int total_tasks = instance->tasks_completed + instance->tasks_failed;
    if (total_tasks == 1) {
        instance->avg_response_time_ms = response_time;
    } else {
        instance->avg_response_time_ms = 
            (instance->avg_response_time_ms * (total_tasks - 1) + response_time) / total_tasks;
    }
    
    pthread_mutex_unlock(&instance->mutex);
}

/* Batch URL check */
int cdp_batch_url_check(const char** urls, int count, int max_concurrent, char* results_json) {
    if (!urls || count <= 0 || !results_json) {
        return CDP_CONCURRENT_ERR_INVALID_PARAM;
    }
    
    /* Create temporary scheduler for batch operation */
    cdp_task_scheduler_t* scheduler;
    int ret = cdp_create_task_scheduler(max_concurrent, &scheduler);
    if (ret != CDP_CONCURRENT_SUCCESS) {
        return ret;
    }
    
    ret = cdp_start_scheduler(scheduler);
    if (ret != CDP_CONCURRENT_SUCCESS) {
        cdp_destroy_task_scheduler(scheduler);
        return ret;
    }
    
    /* Submit tasks for each URL */
    int* task_ids = calloc(count, sizeof(int));
    if (!task_ids) {
        cdp_destroy_task_scheduler(scheduler);
        return CDP_CONCURRENT_ERR_MEMORY;
    }
    
    for (int i = 0; i < count; i++) {
        cdp_task_t* task;
        char task_data[CDP_MAX_TASK_DATA_LENGTH];
        snprintf(task_data, sizeof(task_data), "{\"url\":\"%s\"}", urls[i]);
        
        ret = cdp_create_task("url_check", task_data, &task);
        if (ret == CDP_CONCURRENT_SUCCESS) {
            task_ids[i] = task->task_id;
            cdp_submit_task(scheduler, task);
        }
    }
    
    /* Wait for all tasks to complete */
    cdp_wait_for_all_tasks(60000);
    
    /* Collect results */
    strcpy(results_json, "[");
    for (int i = 0; i < count; i++) {
        if (task_ids[i] > 0) {
            cdp_task_t task_info;
            if (cdp_get_task_status(task_ids[i], &task_info) == CDP_CONCURRENT_SUCCESS) {
                char result_entry[512];
                snprintf(result_entry, sizeof(result_entry),
                    "{\"url\":\"%s\",\"status\":\"%s\",\"result\":%s}",
                    urls[i],
                    cdp_task_status_to_string(task_info.status),
                    task_info.result_data);
                
                if (i > 0) strcat(results_json, ",");
                strcat(results_json, result_entry);
            }
        }
    }
    strcat(results_json, "]");
    
    free(task_ids);
    cdp_stop_scheduler(scheduler);
    cdp_destroy_task_scheduler(scheduler);
    
    return CDP_CONCURRENT_SUCCESS;
}

/* Get performance metrics */
int cdp_get_performance_metrics(cdp_task_scheduler_t* scheduler, cdp_performance_metrics_t* metrics) {
    if (!scheduler || !metrics) {
        return CDP_CONCURRENT_ERR_INVALID_PARAM;
    }
    
    pthread_mutex_lock(&scheduler->mutex);
    memcpy(metrics, &scheduler->metrics, sizeof(cdp_performance_metrics_t));
    
    /* Calculate current metrics */
    metrics->pending_tasks = scheduler->queue->count;
    metrics->active_instances = scheduler->pool->active_count;
    metrics->idle_instances = scheduler->pool->pool_size - scheduler->pool->active_count;
    
    /* Calculate throughput */
    time_t elapsed = time(NULL) - metrics->start_time;
    if (elapsed > 0) {
        metrics->throughput_per_minute = (metrics->completed_tasks * 60.0) / elapsed;
    }
    
    pthread_mutex_unlock(&scheduler->mutex);
    return CDP_CONCURRENT_SUCCESS;
}

/* Utility: Convert task status to string */
const char* cdp_task_status_to_string(cdp_task_status_t status) {
    switch (status) {
        case CDP_TASK_PENDING: return "PENDING";
        case CDP_TASK_QUEUED: return "QUEUED";
        case CDP_TASK_RUNNING: return "RUNNING";
        case CDP_TASK_COMPLETED: return "COMPLETED";
        case CDP_TASK_FAILED: return "FAILED";
        case CDP_TASK_CANCELLED: return "CANCELLED";
        case CDP_TASK_RETRYING: return "RETRYING";
        default: return "UNKNOWN";
    }
}

/* Utility: Convert task priority to string */
const char* cdp_task_priority_to_string(cdp_task_priority_t priority) {
    switch (priority) {
        case CDP_PRIORITY_LOW: return "LOW";
        case CDP_PRIORITY_NORMAL: return "NORMAL";
        case CDP_PRIORITY_HIGH: return "HIGH";
        case CDP_PRIORITY_CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

/* Utility: Convert balance strategy to string */
const char* cdp_balance_strategy_to_string(cdp_balance_strategy_t strategy) {
    switch (strategy) {
        case CDP_BALANCE_ROUND_ROBIN: return "ROUND_ROBIN";
        case CDP_BALANCE_LEAST_LOADED: return "LEAST_LOADED";
        case CDP_BALANCE_PERFORMANCE: return "PERFORMANCE";
        case CDP_BALANCE_RANDOM: return "RANDOM";
        default: return "UNKNOWN";
    }
}

/* Utility: Convert concurrent error to string */
const char* cdp_concurrent_error_to_string(cdp_concurrent_error_t error) {
    switch (error) {
        case CDP_CONCURRENT_SUCCESS: return "Success";
        case CDP_CONCURRENT_ERR_INVALID_PARAM: return "Invalid parameter";
        case CDP_CONCURRENT_ERR_QUEUE_FULL: return "Queue is full";
        case CDP_CONCURRENT_ERR_TASK_NOT_FOUND: return "Task not found";
        case CDP_CONCURRENT_ERR_POOL_EMPTY: return "Pool is empty";
        case CDP_CONCURRENT_ERR_INSTANCE_BUSY: return "Instance is busy";
        case CDP_CONCURRENT_ERR_TIMEOUT: return "Operation timed out";
        case CDP_CONCURRENT_ERR_CANCELLED: return "Task was cancelled";
        case CDP_CONCURRENT_ERR_MAX_RETRIES: return "Maximum retries exceeded";
        case CDP_CONCURRENT_ERR_POOL_INIT_FAILED: return "Pool initialization failed";
        case CDP_CONCURRENT_ERR_MEMORY: return "Memory allocation failed";
        default: return "Unknown error";
    }
}

/* Wait for all tasks to complete */
int cdp_wait_for_all_tasks(int timeout_ms) {
    time_t start_time = time(NULL);
    
    while (1) {
        int all_done = 1;
        
        pthread_mutex_lock(&g_registry_mutex);
        task_registry_node_t* node = g_task_registry;
        while (node) {
            if (node->task) {
                pthread_mutex_lock(&node->task->mutex);
                if (node->task->status == CDP_TASK_PENDING ||
                    node->task->status == CDP_TASK_QUEUED ||
                    node->task->status == CDP_TASK_RUNNING) {
                    all_done = 0;
                }
                pthread_mutex_unlock(&node->task->mutex);
            }
            if (!all_done) break;
            node = node->next;
        }
        pthread_mutex_unlock(&g_registry_mutex);
        
        if (all_done) {
            return CDP_CONCURRENT_SUCCESS;
        }
        
        /* Check timeout */
        if (timeout_ms > 0) {
            time_t elapsed = (time(NULL) - start_time) * 1000;
            if (elapsed >= timeout_ms) {
                return CDP_CONCURRENT_ERR_TIMEOUT;
            }
        }
        
        usleep(100000);  /* Sleep 100ms */
    }
}