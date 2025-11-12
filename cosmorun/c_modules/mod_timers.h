#ifndef COSMORUN_TIMERS_H
#define COSMORUN_TIMERS_H

/*
 * mod_timers - Node.js-style Timers module for cosmorun
 *
 * Provides timer functionality matching Node.js Timers API:
 * - setTimeout(callback, delay, ...args)
 * - setInterval(callback, delay, ...args)
 * - clearTimeout(id)
 * - clearInterval(id)
 * - setImmediate(callback, ...args)
 *
 * Features:
 * - Sorted timer queue for efficient polling
 * - Monotonic clock for reliable timing
 * - Thread-safe timer ID generation
 * - Automatic cleanup of one-shot timers
 * - Minimum 1ms resolution
 */

#include "src/cosmo_libc.h"

/* Timer ID type - unique identifier for each timer */
typedef int timer_id_t;

/* Invalid timer ID constant */
#define TIMER_INVALID_ID 0

/* Timer callback function signature */
typedef void (*timer_callback_fn)(void *ctx);

/* Timer types */
typedef enum {
    TIMER_TYPE_TIMEOUT,    /* One-shot timer (setTimeout) */
    TIMER_TYPE_INTERVAL,   /* Repeating timer (setInterval) */
    TIMER_TYPE_IMMEDIATE   /* Execute ASAP (setImmediate) */
} timer_type_t;

/* Timer structure - internal representation */
typedef struct timer_node {
    timer_id_t id;
    timer_type_t type;
    timer_callback_fn callback;
    void *ctx;
    int64_t interval_us;        /* Interval in microseconds */
    int64_t next_fire_time_us;  /* Next fire time in microseconds */
    struct timer_node *next;    /* Next timer in sorted list */
} timer_node_t;

/* Timer manager - maintains all active timers */
typedef struct {
    timer_node_t *head;         /* Head of sorted timer list */
    timer_id_t next_id;         /* Next available timer ID */
    int count;                  /* Number of active timers */
} timer_manager_t;

/* ==================== Core API ==================== */

/**
 * Initialize the timer manager
 * Must be called before using any timer functions
 */
void timers_init(timer_manager_t *mgr);

/**
 * Cleanup the timer manager
 * Cancels all active timers and frees resources
 */
void timers_cleanup(timer_manager_t *mgr);

/**
 * setTimeout - Execute callback once after delay
 * @param mgr: Timer manager
 * @param callback: Function to call when timer fires
 * @param ctx: Context pointer passed to callback
 * @param delay_ms: Delay in milliseconds (minimum 0)
 * @return: Timer ID (or TIMER_INVALID_ID on error)
 */
timer_id_t timers_set_timeout(timer_manager_t *mgr,
                              timer_callback_fn callback,
                              void *ctx,
                              int delay_ms);

/**
 * setInterval - Execute callback repeatedly at intervals
 * @param mgr: Timer manager
 * @param callback: Function to call when timer fires
 * @param ctx: Context pointer passed to callback
 * @param interval_ms: Interval in milliseconds (minimum 1)
 * @return: Timer ID (or TIMER_INVALID_ID on error)
 */
timer_id_t timers_set_interval(timer_manager_t *mgr,
                               timer_callback_fn callback,
                               void *ctx,
                               int interval_ms);

/**
 * setImmediate - Execute callback as soon as possible
 * @param mgr: Timer manager
 * @param callback: Function to call
 * @param ctx: Context pointer passed to callback
 * @return: Timer ID (or TIMER_INVALID_ID on error)
 */
timer_id_t timers_set_immediate(timer_manager_t *mgr,
                                timer_callback_fn callback,
                                void *ctx);

/**
 * clearTimeout - Cancel a timeout timer
 * @param mgr: Timer manager
 * @param id: Timer ID returned by setTimeout
 */
void timers_clear_timeout(timer_manager_t *mgr, timer_id_t id);

/**
 * clearInterval - Cancel an interval timer
 * @param mgr: Timer manager
 * @param id: Timer ID returned by setInterval
 */
void timers_clear_interval(timer_manager_t *mgr, timer_id_t id);

/**
 * clearImmediate - Cancel an immediate callback
 * @param mgr: Timer manager
 * @param id: Timer ID returned by setImmediate
 */
void timers_clear_immediate(timer_manager_t *mgr, timer_id_t id);

/**
 * timers_process - Process all expired timers
 * Call this function in your event loop to execute timer callbacks
 * @param mgr: Timer manager
 * @return: Number of timers that fired
 */
int timers_process(timer_manager_t *mgr);

/**
 * timers_get_next_timeout - Get time until next timer fires
 * Useful for select()/poll() timeout calculation
 * @param mgr: Timer manager
 * @return: Microseconds until next timer (or -1 if no timers)
 */
int64_t timers_get_next_timeout(timer_manager_t *mgr);

/**
 * timers_count - Get number of active timers
 * @param mgr: Timer manager
 * @return: Number of active timers
 */
int timers_count(timer_manager_t *mgr);

/* ==================== Utility Functions ==================== */

/**
 * timers_get_monotonic_time - Get monotonic clock time in microseconds
 * @return: Current time in microseconds
 */
int64_t timers_get_monotonic_time(void);

#endif /* COSMORUN_TIMERS_H */
