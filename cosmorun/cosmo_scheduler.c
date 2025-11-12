#include "cosmo_scheduler.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <unistd.h>
#include <time.h>
#include <sched.h>

#define DEQUE_CAPACITY 1024
#define STEAL_ATTEMPTS 5

// Task structure
typedef struct {
    task_fn_t func;
    void *arg;
} task_t;

// Simple queue for task submission
typedef struct {
    task_t *tasks;
    int capacity;
    int head;
    int tail;
    int count;
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
} task_queue_t;

// Worker thread state
typedef struct {
    pthread_t thread;
    int id;
    task_queue_t *local_queue;  // Own queue
    struct scheduler_t *sched;

    // Statistics
    atomic_ulong tasks_completed;
    atomic_ulong steals_succeeded;
    atomic_ulong idle_cycles;
    atomic_ulong work_cycles;
} worker_t;

// Scheduler structure
struct scheduler_t {
    worker_t *workers;
    int num_workers;

    atomic_int active_tasks;
    atomic_int shutdown;

    // Global statistics
    atomic_ulong tasks_spawned;
    atomic_ulong steals_attempted;

    pthread_mutex_t wait_lock;
    pthread_cond_t wait_cond;
};

// Thread-local storage
static _Thread_local scheduler_t *tls_scheduler = NULL;
static _Thread_local int tls_worker_id = -1;

// Queue operations
static task_queue_t* queue_create(int capacity) {
    task_queue_t *q = calloc(1, sizeof(task_queue_t));
    if (!q) return NULL;

    q->tasks = calloc(capacity, sizeof(task_t));
    if (!q->tasks) {
        free(q);
        return NULL;
    }

    q->capacity = capacity;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    return q;
}

static void queue_destroy(task_queue_t *q) {
    if (!q) return;
    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_empty);
    free(q->tasks);
    free(q);
}

static int queue_push(task_queue_t *q, task_t task) {
    pthread_mutex_lock(&q->lock);
    if (q->count >= q->capacity) {
        pthread_mutex_unlock(&q->lock);
        return 0;
    }
    q->tasks[q->tail] = task;
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
    return 1;
}

static int queue_pop(task_queue_t *q, task_t *task) {
    pthread_mutex_lock(&q->lock);
    if (q->count == 0) {
        pthread_mutex_unlock(&q->lock);
        return 0;
    }
    *task = q->tasks[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    pthread_mutex_unlock(&q->lock);
    return 1;
}

static int queue_steal(task_queue_t *q, task_t *task) {
    // Try to steal from head (opposite end from owner)
    pthread_mutex_lock(&q->lock);
    if (q->count == 0) {
        pthread_mutex_unlock(&q->lock);
        return 0;
    }
    *task = q->tasks[q->head];
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    pthread_mutex_unlock(&q->lock);
    return 1;
}

// Try to steal from other workers
static int steal_task(worker_t *self, task_t *task) {
    scheduler_t *sched = self->sched;
    int num_workers = sched->num_workers;

    for (int attempt = 0; attempt < STEAL_ATTEMPTS; attempt++) {
        // Random victim
        int victim_id = rand() % num_workers;
        if (victim_id == self->id) {
            victim_id = (victim_id + 1) % num_workers;
        }

        atomic_fetch_add(&sched->steals_attempted, 1);

        if (queue_steal(sched->workers[victim_id].local_queue, task)) {
            atomic_fetch_add(&self->steals_succeeded, 1);
            return 1;
        }
    }

    return 0;
}

// Worker thread main loop
static void* worker_main(void *arg) {
    worker_t *self = (worker_t*)arg;
    scheduler_t *sched = self->sched;

    tls_scheduler = sched;
    tls_worker_id = self->id;

    task_t task;

    while (!atomic_load(&sched->shutdown)) {
        // Try to get task from local queue
        if (queue_pop(self->local_queue, &task)) {
            atomic_fetch_add(&self->work_cycles, 1);
            task.func(task.arg);
            atomic_fetch_add(&self->tasks_completed, 1);

            int remaining = atomic_fetch_sub(&sched->active_tasks, 1) - 1;
            if (remaining == 0) {
                pthread_mutex_lock(&sched->wait_lock);
                pthread_cond_broadcast(&sched->wait_cond);
                pthread_mutex_unlock(&sched->wait_lock);
            }
        } else if (steal_task(self, &task)) {
            // Stolen task
            atomic_fetch_add(&self->work_cycles, 1);
            task.func(task.arg);
            atomic_fetch_add(&self->tasks_completed, 1);

            int remaining = atomic_fetch_sub(&sched->active_tasks, 1) - 1;
            if (remaining == 0) {
                pthread_mutex_lock(&sched->wait_lock);
                pthread_cond_broadcast(&sched->wait_cond);
                pthread_mutex_unlock(&sched->wait_lock);
            }
        } else {
            // No work available
            if (atomic_load(&sched->active_tasks) > 0) {
                atomic_fetch_add(&self->idle_cycles, 1);
                sched_yield();
            } else {
                atomic_fetch_add(&self->idle_cycles, 1);
                struct timespec ts = {0, 10000};  // 10 microseconds
                nanosleep(&ts, NULL);
            }
        }
    }

    return NULL;
}

scheduler_t* scheduler_create(int num_threads) {
    if (num_threads <= 0) {
        num_threads = sysconf(_SC_NPROCESSORS_ONLN);
    }

    scheduler_t *sched = calloc(1, sizeof(scheduler_t));
    if (!sched) return NULL;

    sched->num_workers = num_threads;
    sched->workers = calloc(num_threads, sizeof(worker_t));
    if (!sched->workers) {
        free(sched);
        return NULL;
    }

    atomic_init(&sched->active_tasks, 0);
    atomic_init(&sched->shutdown, 0);
    atomic_init(&sched->tasks_spawned, 0);
    atomic_init(&sched->steals_attempted, 0);

    pthread_mutex_init(&sched->wait_lock, NULL);
    pthread_cond_init(&sched->wait_cond, NULL);

    // Initialize workers (create queues first, then start threads)
    for (int i = 0; i < num_threads; i++) {
        worker_t *w = &sched->workers[i];
        w->id = i;
        w->sched = sched;
        w->local_queue = queue_create(DEQUE_CAPACITY);
        if (!w->local_queue) {
            // Cleanup
            for (int j = 0; j < i; j++) {
                queue_destroy(sched->workers[j].local_queue);
            }
            free(sched->workers);
            free(sched);
            return NULL;
        }

        atomic_init(&w->tasks_completed, 0);
        atomic_init(&w->steals_succeeded, 0);
        atomic_init(&w->idle_cycles, 0);
        atomic_init(&w->work_cycles, 0);
    }

    // Now start all worker threads
    for (int i = 0; i < num_threads; i++) {
        worker_t *w = &sched->workers[i];
        if (pthread_create(&w->thread, NULL, worker_main, w) != 0) {
            // Cleanup on failure
            atomic_store(&sched->shutdown, 1);
            // Join already started threads
            for (int j = 0; j < i; j++) {
                pthread_join(sched->workers[j].thread, NULL);
            }
            // Destroy all queues
            for (int j = 0; j < num_threads; j++) {
                queue_destroy(sched->workers[j].local_queue);
            }
            free(sched->workers);
            free(sched);
            return NULL;
        }
    }

    return sched;
}

void scheduler_submit(scheduler_t *sched, task_fn_t task, void *arg) {
    task_t t = {task, arg};

    atomic_fetch_add(&sched->tasks_spawned, 1);
    atomic_fetch_add(&sched->active_tasks, 1);

    // If called from worker thread, push to local queue
    if (tls_worker_id >= 0 && tls_scheduler == sched) {
        if (!queue_push(sched->workers[tls_worker_id].local_queue, t)) {
            // Queue full, try another worker
            int target = (tls_worker_id + 1) % sched->num_workers;
            queue_push(sched->workers[target].local_queue, t);
        }
    } else {
        // External submission, round-robin to workers
        static atomic_int next_worker = ATOMIC_VAR_INIT(0);
        int target = atomic_fetch_add(&next_worker, 1) % sched->num_workers;
        queue_push(sched->workers[target].local_queue, t);
    }
}

void scheduler_wait(scheduler_t *sched) {
    pthread_mutex_lock(&sched->wait_lock);
    while (atomic_load(&sched->active_tasks) > 0) {
        pthread_cond_wait(&sched->wait_cond, &sched->wait_lock);
    }
    pthread_mutex_unlock(&sched->wait_lock);
}

void scheduler_stats(scheduler_t *sched, scheduler_stats_t *stats) {
    memset(stats, 0, sizeof(scheduler_stats_t));

    stats->tasks_spawned = atomic_load(&sched->tasks_spawned);
    stats->steals_attempted = atomic_load(&sched->steals_attempted);

    unsigned long total_completed = 0;
    unsigned long total_steals = 0;
    unsigned long total_idle = 0;
    unsigned long total_work = 0;

    for (int i = 0; i < sched->num_workers; i++) {
        total_completed += atomic_load(&sched->workers[i].tasks_completed);
        total_steals += atomic_load(&sched->workers[i].steals_succeeded);
        total_idle += atomic_load(&sched->workers[i].idle_cycles);
        total_work += atomic_load(&sched->workers[i].work_cycles);
    }

    stats->tasks_completed = total_completed;
    stats->steals_succeeded = total_steals;
    stats->idle_cycles = total_idle;

    // CPU utilization = work_cycles / (work_cycles + idle_cycles)
    unsigned long total_cycles = total_work + total_idle;
    if (total_cycles > 0) {
        stats->cpu_utilization = (double)total_work / total_cycles;
    }
}

void scheduler_destroy(scheduler_t *sched) {
    if (!sched) return;

    // Signal shutdown
    atomic_store(&sched->shutdown, 1);

    // Wait for all workers
    for (int i = 0; i < sched->num_workers; i++) {
        pthread_join(sched->workers[i].thread, NULL);
        queue_destroy(sched->workers[i].local_queue);
    }

    pthread_mutex_destroy(&sched->wait_lock);
    pthread_cond_destroy(&sched->wait_cond);

    free(sched->workers);
    free(sched);
}

scheduler_t* scheduler_current(void) {
    return tls_scheduler;
}
