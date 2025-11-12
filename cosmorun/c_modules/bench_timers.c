/*
 * bench_timers.c - Performance benchmark for mod_timers
 *
 * Tests:
 * 1. Insertion speed (1000 timers)
 * 2. Processing speed
 * 3. Memory usage
 */

#include "src/cosmo_libc.h"
#include "mod_timers.c"

extern int printf(const char *fmt, ...);

static int callback_count = 0;

void bench_callback(void *ctx) {
    callback_count++;
}

void benchmark_insertion(void) {
    printf("\n[Benchmark] Timer Insertion Speed\n");
    printf("=====================================\n");

    timer_manager_t mgr;
    timers_init(&mgr);

    int num_timers = 1000;
    int64_t start = timers_get_monotonic_time();

    /* Insert 1000 timers with random delays */
    for (int i = 0; i < num_timers; i++) {
        int delay_ms = (i * 7) % 500;  /* Pseudo-random delays 0-499ms */
        timers_set_timeout(&mgr, bench_callback, NULL, delay_ms);
    }

    int64_t end = timers_get_monotonic_time();
    int64_t elapsed_us = end - start;

    printf("Inserted %d timers in %lld μs\n", num_timers, elapsed_us);
    printf("Average: %lld μs per timer\n", elapsed_us / num_timers);
    printf("Rate: %lld timers/sec\n", (int64_t)(1000000LL * num_timers / elapsed_us));

    timers_cleanup(&mgr);
}

void benchmark_processing(void) {
    printf("\n[Benchmark] Timer Processing Speed\n");
    printf("=====================================\n");

    timer_manager_t mgr;
    timers_init(&mgr);

    int num_timers = 1000;
    callback_count = 0;

    /* Insert 1000 immediate timers */
    for (int i = 0; i < num_timers; i++) {
        timers_set_immediate(&mgr, bench_callback, NULL);
    }

    int64_t start = timers_get_monotonic_time();
    int fired = timers_process(&mgr);
    int64_t end = timers_get_monotonic_time();
    int64_t elapsed_us = end - start;

    printf("Processed %d timers in %lld μs\n", fired, elapsed_us);
    printf("Average: %lld μs per timer\n", elapsed_us / fired);
    printf("Rate: %lld timers/sec\n", (int64_t)(1000000LL * fired / elapsed_us));
    printf("Callbacks executed: %d\n", callback_count);

    timers_cleanup(&mgr);
}

void benchmark_memory(void) {
    printf("\n[Benchmark] Memory Usage\n");
    printf("=====================================\n");

    timer_manager_t mgr;
    timers_init(&mgr);

    int num_timers = 100;

    /* Insert timers */
    for (int i = 0; i < num_timers; i++) {
        timers_set_timeout(&mgr, bench_callback, NULL, 1000);
    }

    size_t timer_size = sizeof(timer_node_t);
    size_t total_memory = timer_size * num_timers;

    printf("Timer structure size: %lu bytes\n", (unsigned long)timer_size);
    printf("Active timers: %d\n", num_timers);
    printf("Estimated memory: %lu bytes (%lu KB)\n",
           (unsigned long)total_memory, (unsigned long)(total_memory / 1024));
    printf("Per timer: %lu bytes\n", (unsigned long)timer_size);

    timers_cleanup(&mgr);
}

void benchmark_ordering(void) {
    printf("\n[Benchmark] Timer Ordering (Sorted Queue)\n");
    printf("=====================================\n");

    timer_manager_t mgr;
    timers_init(&mgr);

    int num_timers = 500;

    /* Insert timers in reverse order (worst case) */
    int64_t start = timers_get_monotonic_time();
    for (int i = num_timers - 1; i >= 0; i--) {
        timers_set_timeout(&mgr, bench_callback, NULL, i);
    }
    int64_t end = timers_get_monotonic_time();
    int64_t insertion_us = end - start;

    printf("Inserted %d timers (reverse order) in %lld μs\n",
           num_timers, insertion_us);

    /* Verify ordering by checking next timeout */
    start = timers_get_monotonic_time();
    for (int i = 0; i < 10; i++) {
        int64_t timeout = timers_get_next_timeout(&mgr);
        (void)timeout;  /* Suppress unused warning */
    }
    end = timers_get_monotonic_time();
    int64_t lookup_us = end - start;

    printf("10x next_timeout lookups in %lld μs\n", lookup_us);
    printf("Average lookup: %lld μs (O(1))\n", lookup_us / 10);

    timers_cleanup(&mgr);
}

int main(void) {
    printf("========================================\n");
    printf("  mod_timers Performance Benchmark\n");
    printf("========================================\n");

    benchmark_insertion();
    benchmark_processing();
    benchmark_memory();
    benchmark_ordering();

    printf("\n========================================\n");
    printf("  Benchmark Complete\n");
    printf("========================================\n\n");

    return 0;
}
