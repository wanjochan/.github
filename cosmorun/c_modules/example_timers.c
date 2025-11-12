/*
 * example_timers.c - Example usage of mod_timers
 *
 * Demonstrates:
 * - setTimeout for delayed execution
 * - setInterval for periodic tasks
 * - setImmediate for ASAP execution
 * - Timer cancellation
 * - Event loop integration
 */

#include "src/cosmo_libc.h"
#include "mod_timers.c"

/* Example callbacks */
void on_timeout(void *ctx) {
    printf("⏰ Timeout fired! Context: %s\n", (char*)ctx);
}

void on_interval(void *ctx) {
    static int count = 0;
    count++;
    printf("🔄 Interval tick #%d (ctx: %s)\n", count, (char*)ctx);
}

void on_immediate(void *ctx) {
    printf("⚡ Immediate callback! Context: %s\n", (char*)ctx);
}

/* Main event loop example */
int main(void) {
    printf("========================================\n");
    printf("  mod_timers Example\n");
    printf("========================================\n\n");

    /* Initialize timer manager */
    timer_manager_t mgr;
    timers_init(&mgr);

    /* Example 1: setTimeout - one-shot timer */
    printf("1. Setting timeout for 100ms...\n");
    timer_id_t timeout_id = timers_set_timeout(&mgr, on_timeout,
                                               (void*)"one-shot", 100);
    printf("   Timer ID: %d\n\n", timeout_id);

    /* Example 2: setInterval - repeating timer */
    printf("2. Setting interval for 50ms (will run 3 times)...\n");
    timer_id_t interval_id = timers_set_interval(&mgr, on_interval,
                                                 (void*)"periodic", 50);
    printf("   Timer ID: %d\n\n", interval_id);

    /* Example 3: setImmediate - execute ASAP */
    printf("3. Setting immediate callback...\n");
    timer_id_t imm_id = timers_set_immediate(&mgr, on_immediate,
                                            (void*)"urgent");
    printf("   Timer ID: %d\n\n", imm_id);

    /* Example 4: Check active timers */
    printf("Active timers: %d\n\n", timers_count(&mgr));

    /* Event loop - process timers */
    printf("Starting event loop...\n");
    printf("========================================\n\n");

    int iterations = 0;
    int interval_fires = 0;

    while (timers_count(&mgr) > 0 && iterations < 20) {
        /* Get time until next timer */
        int64_t timeout_us = timers_get_next_timeout(&mgr);

        if (timeout_us > 0) {
            /* Sleep until next timer (or poll interval) */
            int sleep_us = (timeout_us < 10000) ? timeout_us : 10000;
            usleep(sleep_us);
        }

        /* Process expired timers */
        int fired = timers_process(&mgr);
        if (fired > 0) {
            printf("   [%d timers fired, %d active]\n\n",
                   fired, timers_count(&mgr));

            /* Count interval fires and stop after 3 */
            interval_fires += fired;
            if (interval_fires >= 4) {  /* 3 interval + 1 timeout */
                printf("4. Clearing interval timer (ID: %d)...\n", interval_id);
                timers_clear_interval(&mgr, interval_id);
                printf("   Interval stopped.\n\n");
            }
        }

        iterations++;
    }

    printf("========================================\n");
    printf("Event loop finished after %d iterations\n", iterations);
    printf("Active timers remaining: %d\n", timers_count(&mgr));

    /* Cleanup */
    timers_cleanup(&mgr);
    printf("\n✅ Timer manager cleaned up\n");

    return 0;
}
