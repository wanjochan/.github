#ifndef COSMO_PROFILER_H
#define COSMO_PROFILER_H

#include <stdint.h>
#include <time.h>

/* Instrumentation profiling entry */
typedef struct {
    const char *function_name;
    uint64_t call_count;
    uint64_t total_time_ns;    // Nanoseconds
    uint64_t min_time_ns;
    uint64_t max_time_ns;
    double avg_time_ns;
} instrument_entry_t;

/* Hash table entry for instrumentation data */
typedef struct instrument_node_t {
    void *func_addr;
    char *function_name;
    uint64_t call_count;
    uint64_t total_time_ns;
    uint64_t min_time_ns;
    uint64_t max_time_ns;
    uint64_t start_time_ns;    // For tracking nested calls
    int depth;                  // Call depth for nested functions
    struct instrument_node_t *next;
} instrument_node_t;

/* Profiler structure */
typedef struct {
    int instrumentation_enabled;
    instrument_node_t **hash_table;
    int hash_table_size;
    int total_functions;
} profiler_t;

/* Initialize profiler */
profiler_t* profiler_create(void);

/* Enable instrumentation profiling */
int profiler_enable_instrumentation(profiler_t *prof);

/* Get instrumentation results */
int profiler_get_instrumentation_results(profiler_t *prof, instrument_entry_t **entries, int *count);

/* Print instrumentation report */
void profiler_print_report(profiler_t *prof);

/* Cleanup profiler */
void profiler_destroy(profiler_t *prof);

/* GCC instrumentation hooks (marked no_instrument to avoid recursion) */
void __cyg_profile_func_enter(void *func, void *caller) __attribute__((no_instrument_function));
void __cyg_profile_func_exit(void *func, void *caller) __attribute__((no_instrument_function));

/* Global profiler instance for instrumentation hooks */
extern profiler_t *g_profiler;

#endif /* COSMO_PROFILER_H */
