/* cosmo_parallel.h - Thread Pool for Parallel Compilation
 * Provides fixed-size thread pool for compiling multiple modules in parallel
 */

#ifndef COSMO_PARALLEL_H
#define COSMO_PARALLEL_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque thread pool type
typedef struct thread_pool_t thread_pool_t;

// Task function type
typedef void (*task_func_t)(void *arg);

// Thread pool API
thread_pool_t* thread_pool_create(int num_threads);
void thread_pool_submit(thread_pool_t *pool, task_func_t task, void *arg);
void thread_pool_wait(thread_pool_t *pool);  // Wait for all tasks to complete
void thread_pool_destroy(thread_pool_t *pool);

// Helper to get default thread count (number of CPU cores)
int get_default_thread_count(void);

#ifdef __cplusplus
}
#endif

#endif // COSMO_PARALLEL_H
