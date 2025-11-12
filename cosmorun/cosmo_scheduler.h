#ifndef COSMO_SCHEDULER_H
#define COSMO_SCHEDULER_H

#include <stddef.h>

// Task function signature
typedef void (*task_fn_t)(void *arg);

// Opaque scheduler handle
typedef struct scheduler_t scheduler_t;

// Statistics for scheduler performance
typedef struct {
    unsigned long tasks_completed;
    unsigned long tasks_spawned;
    unsigned long steals_attempted;
    unsigned long steals_succeeded;
    unsigned long idle_cycles;
    double cpu_utilization;  // 0.0 to 1.0
} scheduler_stats_t;

// Create scheduler with specified number of worker threads
// Returns NULL on failure
scheduler_t* scheduler_create(int num_threads);

// Submit a task to the scheduler
// Thread-safe, can be called from any thread including task functions
void scheduler_submit(scheduler_t *sched, task_fn_t task, void *arg);

// Wait for all tasks to complete
// Blocks until all submitted tasks and their spawned tasks finish
void scheduler_wait(scheduler_t *sched);

// Get current scheduler statistics
// Safe to call while scheduler is running
void scheduler_stats(scheduler_t *sched, scheduler_stats_t *stats);

// Destroy scheduler and free resources
// Must be called after scheduler_wait()
void scheduler_destroy(scheduler_t *sched);

// Get scheduler instance for current thread (if running inside a task)
// Returns NULL if not in a worker thread
scheduler_t* scheduler_current(void);

#endif // COSMO_SCHEDULER_H
