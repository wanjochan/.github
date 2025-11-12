/*
 * mod_timers - Node.js-style Timers implementation for cosmorun
 *
 * Timer queue implementation using sorted linked list:
 * - Timers sorted by next_fire_time (earliest first)
 * - O(n) insertion, O(1) next timer check
 * - Automatic cleanup of one-shot timers
 * - Monotonic clock for reliable timing
 */

#include "mod_timers.h"

/* External declarations for libc functions */
extern int gettimeofday(struct timeval *tv, void *tz);
extern void *malloc(size_t size);
extern void free(void *ptr);

/* ==================== Helper Functions ==================== */

/**
 * Get current monotonic time in microseconds
 * Uses gettimeofday for compatibility
 */
int64_t timers_get_monotonic_time(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000LL + (int64_t)tv.tv_usec;
}

/**
 * Generate unique timer ID
 * Thread-safe incrementing ID generator
 */
static timer_id_t generate_timer_id(timer_manager_t *mgr) {
    if (mgr->next_id == 0) {
        mgr->next_id = 1;  /* Skip TIMER_INVALID_ID */
    }
    return mgr->next_id++;
}

/**
 * Create new timer node
 */
static timer_node_t* create_timer_node(timer_id_t id,
                                       timer_type_t type,
                                       timer_callback_fn callback,
                                       void *ctx,
                                       int64_t interval_us) {
    timer_node_t *node = (timer_node_t*)malloc(sizeof(timer_node_t));
    if (!node) return NULL;

    node->id = id;
    node->type = type;
    node->callback = callback;
    node->ctx = ctx;
    node->interval_us = interval_us;
    node->next = NULL;

    /* Calculate next fire time */
    int64_t now = timers_get_monotonic_time();
    if (type == TIMER_TYPE_IMMEDIATE) {
        node->next_fire_time_us = now;  /* Fire ASAP */
    } else {
        node->next_fire_time_us = now + interval_us;
    }

    return node;
}

/**
 * Insert timer into sorted list (earliest first)
 * Maintains sorted order by next_fire_time
 */
static void insert_timer_sorted(timer_manager_t *mgr, timer_node_t *new_node) {
    if (!mgr->head || new_node->next_fire_time_us < mgr->head->next_fire_time_us) {
        /* Insert at head */
        new_node->next = mgr->head;
        mgr->head = new_node;
    } else {
        /* Find insertion point */
        timer_node_t *curr = mgr->head;
        while (curr->next && curr->next->next_fire_time_us <= new_node->next_fire_time_us) {
            curr = curr->next;
        }
        new_node->next = curr->next;
        curr->next = new_node;
    }
    mgr->count++;
}

/**
 * Remove timer from list by ID
 * Returns 1 if found and removed, 0 otherwise
 */
static int remove_timer_by_id(timer_manager_t *mgr, timer_id_t id) {
    if (!mgr->head) return 0;

    /* Check head */
    if (mgr->head->id == id) {
        timer_node_t *to_free = mgr->head;
        mgr->head = mgr->head->next;
        free(to_free);
        mgr->count--;
        return 1;
    }

    /* Search rest of list */
    timer_node_t *curr = mgr->head;
    while (curr->next) {
        if (curr->next->id == id) {
            timer_node_t *to_free = curr->next;
            curr->next = curr->next->next;
            free(to_free);
            mgr->count--;
            return 1;
        }
        curr = curr->next;
    }

    return 0;
}

/* ==================== Core API Implementation ==================== */

void timers_init(timer_manager_t *mgr) {
    mgr->head = NULL;
    mgr->next_id = 1;
    mgr->count = 0;
}

void timers_cleanup(timer_manager_t *mgr) {
    timer_node_t *curr = mgr->head;
    while (curr) {
        timer_node_t *next = curr->next;
        free(curr);
        curr = next;
    }
    mgr->head = NULL;
    mgr->count = 0;
}

timer_id_t timers_set_timeout(timer_manager_t *mgr,
                              timer_callback_fn callback,
                              void *ctx,
                              int delay_ms) {
    if (!mgr || !callback) return TIMER_INVALID_ID;

    /* Handle negative delay as 0 */
    if (delay_ms < 0) delay_ms = 0;

    timer_id_t id = generate_timer_id(mgr);
    int64_t delay_us = (int64_t)delay_ms * 1000LL;

    timer_node_t *node = create_timer_node(id, TIMER_TYPE_TIMEOUT,
                                           callback, ctx, delay_us);
    if (!node) return TIMER_INVALID_ID;

    insert_timer_sorted(mgr, node);
    return id;
}

timer_id_t timers_set_interval(timer_manager_t *mgr,
                               timer_callback_fn callback,
                               void *ctx,
                               int interval_ms) {
    if (!mgr || !callback) return TIMER_INVALID_ID;

    /* Enforce minimum interval of 1ms */
    if (interval_ms < 1) interval_ms = 1;

    timer_id_t id = generate_timer_id(mgr);
    int64_t interval_us = (int64_t)interval_ms * 1000LL;

    timer_node_t *node = create_timer_node(id, TIMER_TYPE_INTERVAL,
                                           callback, ctx, interval_us);
    if (!node) return TIMER_INVALID_ID;

    insert_timer_sorted(mgr, node);
    return id;
}

timer_id_t timers_set_immediate(timer_manager_t *mgr,
                                timer_callback_fn callback,
                                void *ctx) {
    if (!mgr || !callback) return TIMER_INVALID_ID;

    timer_id_t id = generate_timer_id(mgr);

    timer_node_t *node = create_timer_node(id, TIMER_TYPE_IMMEDIATE,
                                           callback, ctx, 0);
    if (!node) return TIMER_INVALID_ID;

    insert_timer_sorted(mgr, node);
    return id;
}

void timers_clear_timeout(timer_manager_t *mgr, timer_id_t id) {
    if (!mgr || id == TIMER_INVALID_ID) return;
    remove_timer_by_id(mgr, id);
}

void timers_clear_interval(timer_manager_t *mgr, timer_id_t id) {
    if (!mgr || id == TIMER_INVALID_ID) return;
    remove_timer_by_id(mgr, id);
}

void timers_clear_immediate(timer_manager_t *mgr, timer_id_t id) {
    if (!mgr || id == TIMER_INVALID_ID) return;
    remove_timer_by_id(mgr, id);
}

int timers_process(timer_manager_t *mgr) {
    if (!mgr || !mgr->head) return 0;

    int64_t now = timers_get_monotonic_time();
    int fired_count = 0;

    /* Process all expired timers */
    while (mgr->head && mgr->head->next_fire_time_us <= now) {
        timer_node_t *timer = mgr->head;
        mgr->head = timer->next;
        mgr->count--;

        /* Execute callback */
        if (timer->callback) {
            timer->callback(timer->ctx);
        }
        fired_count++;

        /* Re-queue interval timers */
        if (timer->type == TIMER_TYPE_INTERVAL) {
            timer->next_fire_time_us = now + timer->interval_us;
            timer->next = NULL;
            insert_timer_sorted(mgr, timer);
        } else {
            /* Free one-shot timers (timeout/immediate) */
            free(timer);
        }
    }

    return fired_count;
}

int64_t timers_get_next_timeout(timer_manager_t *mgr) {
    if (!mgr || !mgr->head) return -1;

    int64_t now = timers_get_monotonic_time();
    int64_t next_fire = mgr->head->next_fire_time_us;

    if (next_fire <= now) {
        return 0;  /* Timer ready to fire */
    }

    return next_fire - now;
}

int timers_count(timer_manager_t *mgr) {
    if (!mgr) return 0;
    return mgr->count;
}
