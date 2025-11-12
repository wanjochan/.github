/* cosmo_parallel.c - Thread Pool Implementation
 * Fixed-size thread pool with task queue and pthread-based workers
 */

#include "cosmo_parallel.h"
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

// Task queue node
typedef struct task_node_t {
    task_func_t task;
    void *arg;
    struct task_node_t *next;
} task_node_t;

// Thread pool structure
struct thread_pool_t {
    pthread_t *threads;
    int num_threads;

    // Task queue (FIFO)
    task_node_t *queue_head;
    task_node_t *queue_tail;

    // Synchronization
    pthread_mutex_t queue_mutex;
    pthread_cond_t queue_cond;
    pthread_cond_t done_cond;

    // State tracking
    int active_tasks;    // Tasks currently being executed
    int pending_tasks;   // Tasks in queue
    int shutdown;        // Shutdown flag
};

// Worker thread function
static void* worker_thread(void *arg) {
    thread_pool_t *pool = (thread_pool_t*)arg;

    while (1) {
        pthread_mutex_lock(&pool->queue_mutex);

        // Wait for tasks or shutdown
        while (pool->queue_head == NULL && !pool->shutdown) {
            pthread_cond_wait(&pool->queue_cond, &pool->queue_mutex);
        }

        // Check for shutdown
        if (pool->shutdown && pool->queue_head == NULL) {
            pthread_mutex_unlock(&pool->queue_mutex);
            break;
        }

        // Dequeue task
        task_node_t *node = pool->queue_head;
        if (node) {
            pool->queue_head = node->next;
            if (pool->queue_tail == node) {
                pool->queue_tail = NULL;
            }
            pool->pending_tasks--;
            pool->active_tasks++;
        }

        pthread_mutex_unlock(&pool->queue_mutex);

        // Execute task
        if (node) {
            node->task(node->arg);
            free(node);

            // Mark task as complete
            pthread_mutex_lock(&pool->queue_mutex);
            pool->active_tasks--;
            if (pool->active_tasks == 0 && pool->pending_tasks == 0) {
                pthread_cond_signal(&pool->done_cond);
            }
            pthread_mutex_unlock(&pool->queue_mutex);
        }
    }

    return NULL;
}

// Get default thread count (number of CPU cores)
int get_default_thread_count(void) {
    long nproc = sysconf(_SC_NPROCESSORS_ONLN);
    if (nproc <= 0) {
        return 4;  // Fallback to 4 threads
    }
    return (int)nproc;
}

// Create thread pool
thread_pool_t* thread_pool_create(int num_threads) {
    if (num_threads <= 0) {
        num_threads = get_default_thread_count();
    }

    thread_pool_t *pool = (thread_pool_t*)calloc(1, sizeof(thread_pool_t));
    if (!pool) {
        return NULL;
    }

    pool->num_threads = num_threads;
    pool->threads = (pthread_t*)calloc(num_threads, sizeof(pthread_t));
    if (!pool->threads) {
        free(pool);
        return NULL;
    }

    // Initialize synchronization primitives
    pthread_mutex_init(&pool->queue_mutex, NULL);
    pthread_cond_init(&pool->queue_cond, NULL);
    pthread_cond_init(&pool->done_cond, NULL);

    // Create worker threads
    for (int i = 0; i < num_threads; i++) {
        if (pthread_create(&pool->threads[i], NULL, worker_thread, pool) != 0) {
            // Cleanup on failure
            pool->shutdown = 1;
            pthread_cond_broadcast(&pool->queue_cond);
            for (int j = 0; j < i; j++) {
                pthread_join(pool->threads[j], NULL);
            }
            free(pool->threads);
            pthread_mutex_destroy(&pool->queue_mutex);
            pthread_cond_destroy(&pool->queue_cond);
            pthread_cond_destroy(&pool->done_cond);
            free(pool);
            return NULL;
        }
    }

    return pool;
}

// Submit task to pool
void thread_pool_submit(thread_pool_t *pool, task_func_t task, void *arg) {
    if (!pool || !task) {
        return;
    }

    task_node_t *node = (task_node_t*)malloc(sizeof(task_node_t));
    if (!node) {
        return;
    }

    node->task = task;
    node->arg = arg;
    node->next = NULL;

    pthread_mutex_lock(&pool->queue_mutex);

    // Enqueue task
    if (pool->queue_tail) {
        pool->queue_tail->next = node;
    } else {
        pool->queue_head = node;
    }
    pool->queue_tail = node;
    pool->pending_tasks++;

    // Signal worker threads
    pthread_cond_signal(&pool->queue_cond);
    pthread_mutex_unlock(&pool->queue_mutex);
}

// Wait for all tasks to complete
void thread_pool_wait(thread_pool_t *pool) {
    if (!pool) {
        return;
    }

    pthread_mutex_lock(&pool->queue_mutex);
    while (pool->active_tasks > 0 || pool->pending_tasks > 0) {
        pthread_cond_wait(&pool->done_cond, &pool->queue_mutex);
    }
    pthread_mutex_unlock(&pool->queue_mutex);
}

// Destroy thread pool
void thread_pool_destroy(thread_pool_t *pool) {
    if (!pool) {
        return;
    }

    // Signal shutdown
    pthread_mutex_lock(&pool->queue_mutex);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->queue_cond);
    pthread_mutex_unlock(&pool->queue_mutex);

    // Join all threads
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    // Free task queue
    task_node_t *node = pool->queue_head;
    while (node) {
        task_node_t *next = node->next;
        free(node);
        node = next;
    }

    // Cleanup
    free(pool->threads);
    pthread_mutex_destroy(&pool->queue_mutex);
    pthread_cond_destroy(&pool->queue_cond);
    pthread_cond_destroy(&pool->done_cond);
    free(pool);
}
