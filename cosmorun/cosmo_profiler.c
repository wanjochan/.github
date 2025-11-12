#ifndef _WIN32
#define _GNU_SOURCE
#endif

#include "cosmo_profiler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#if !defined(_WIN32) && !defined(__COSMOPOLITAN__)
#include <dlfcn.h>
#endif

#define HASH_TABLE_SIZE 1024
#define HASH_MULTIPLIER 31

/* Global profiler instance for instrumentation hooks */
profiler_t *g_profiler = NULL;
static pthread_mutex_t profiler_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Get high-precision time in nanoseconds */
static uint64_t get_time_ns(void) __attribute__((no_instrument_function));
static uint64_t get_time_ns(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/* Hash function for function addresses */
static unsigned int hash_func_addr(void *addr, int table_size) __attribute__((no_instrument_function));
static unsigned int hash_func_addr(void *addr, int table_size) {
    uintptr_t val = (uintptr_t)addr;
    unsigned int hash = 0;

    /* Simple hash mixing */
    hash = (unsigned int)(val ^ (val >> 32));
    hash = hash * HASH_MULTIPLIER;

    return hash % table_size;
}

/* Find or create hash table entry */
static instrument_node_t* find_or_create_node(profiler_t *prof, void *func_addr) __attribute__((no_instrument_function));
static instrument_node_t* find_or_create_node(profiler_t *prof, void *func_addr) {
    if (!prof || !prof->hash_table) return NULL;

    unsigned int idx = hash_func_addr(func_addr, prof->hash_table_size);
    instrument_node_t *node = prof->hash_table[idx];

    /* Search for existing node */
    while (node) {
        if (node->func_addr == func_addr) {
            return node;
        }
        node = node->next;
    }

    /* Create new node */
    node = (instrument_node_t*)calloc(1, sizeof(instrument_node_t));
    if (!node) return NULL;

    node->func_addr = func_addr;
    node->min_time_ns = UINT64_MAX;
    node->max_time_ns = 0;
    node->depth = 0;

    /* Try to resolve function name using dladdr */
#if !defined(_WIN32) && !defined(__COSMOPOLITAN__)
    Dl_info info;
    if (dladdr(func_addr, &info) && info.dli_sname) {
        node->function_name = strdup(info.dli_sname);
    } else
#endif
    {
        /* Fallback to address string */
        char addr_str[32];
        snprintf(addr_str, sizeof(addr_str), "0x%lx", (unsigned long)func_addr);
        node->function_name = strdup(addr_str);
    }

    /* Insert at head of chain */
    node->next = prof->hash_table[idx];
    prof->hash_table[idx] = node;
    prof->total_functions++;

    return node;
}

/* Initialize profiler */
profiler_t* profiler_create(void) {
    profiler_t *prof = (profiler_t*)calloc(1, sizeof(profiler_t));
    if (!prof) return NULL;

    prof->hash_table_size = HASH_TABLE_SIZE;
    prof->hash_table = (instrument_node_t**)calloc(HASH_TABLE_SIZE, sizeof(instrument_node_t*));
    if (!prof->hash_table) {
        free(prof);
        return NULL;
    }

    prof->instrumentation_enabled = 0;
    prof->total_functions = 0;

    return prof;
}

/* Enable instrumentation profiling */
int profiler_enable_instrumentation(profiler_t *prof) {
    if (!prof) return -1;

    pthread_mutex_lock(&profiler_mutex);
    prof->instrumentation_enabled = 1;
    g_profiler = prof;
    pthread_mutex_unlock(&profiler_mutex);

    return 0;
}

/* GCC instrumentation hook - function entry */
void __cyg_profile_func_enter(void *func, void *caller) {
    (void)caller; /* Unused */

    if (!g_profiler || !g_profiler->instrumentation_enabled) return;

    pthread_mutex_lock(&profiler_mutex);

    instrument_node_t *node = find_or_create_node(g_profiler, func);
    if (node) {
        node->depth++;
        /* Always record start time - we track all calls */
        node->start_time_ns = get_time_ns();
    }

    pthread_mutex_unlock(&profiler_mutex);
}

/* GCC instrumentation hook - function exit */
void __cyg_profile_func_exit(void *func, void *caller) {
    (void)caller; /* Unused */

    if (!g_profiler || !g_profiler->instrumentation_enabled) return;

    pthread_mutex_lock(&profiler_mutex);

    unsigned int idx = hash_func_addr(func, g_profiler->hash_table_size);
    instrument_node_t *node = g_profiler->hash_table[idx];

    /* Find existing node */
    while (node) {
        if (node->func_addr == func) {
            node->depth--;

            /* Calculate elapsed time for this call */
            uint64_t end_time = get_time_ns();
            uint64_t elapsed = end_time - node->start_time_ns;

            /* Count every call */
            node->call_count++;
            node->total_time_ns += elapsed;

            if (elapsed < node->min_time_ns) {
                node->min_time_ns = elapsed;
            }
            if (elapsed > node->max_time_ns) {
                node->max_time_ns = elapsed;
            }
            break;
        }
        node = node->next;
    }

    pthread_mutex_unlock(&profiler_mutex);
}

/* Get instrumentation results */
int profiler_get_instrumentation_results(profiler_t *prof, instrument_entry_t **entries, int *count) {
    if (!prof || !entries || !count) return -1;

    pthread_mutex_lock(&profiler_mutex);

    /* Allocate result array */
    *entries = (instrument_entry_t*)calloc(prof->total_functions, sizeof(instrument_entry_t));
    if (!*entries) {
        pthread_mutex_unlock(&profiler_mutex);
        return -1;
    }

    /* Collect all entries */
    int idx = 0;
    for (int i = 0; i < prof->hash_table_size; i++) {
        instrument_node_t *node = prof->hash_table[i];
        while (node) {
            if (node->call_count > 0) {
                (*entries)[idx].function_name = node->function_name;
                (*entries)[idx].call_count = node->call_count;
                (*entries)[idx].total_time_ns = node->total_time_ns;
                (*entries)[idx].min_time_ns = node->min_time_ns;
                (*entries)[idx].max_time_ns = node->max_time_ns;
                (*entries)[idx].avg_time_ns = (double)node->total_time_ns / node->call_count;
                idx++;
            }
            node = node->next;
        }
    }

    *count = idx;
    pthread_mutex_unlock(&profiler_mutex);

    return 0;
}

/* Comparison function for sorting by total time (descending) */
static int compare_by_total_time(const void *a, const void *b) {
    const instrument_entry_t *ea = (const instrument_entry_t*)a;
    const instrument_entry_t *eb = (const instrument_entry_t*)b;

    if (ea->total_time_ns > eb->total_time_ns) return -1;
    if (ea->total_time_ns < eb->total_time_ns) return 1;
    return 0;
}

/* Print instrumentation report */
void profiler_print_report(profiler_t *prof) {
    if (!prof) return;

    instrument_entry_t *entries = NULL;
    int count = 0;

    if (profiler_get_instrumentation_results(prof, &entries, &count) != 0) {
        fprintf(stderr, "Failed to get instrumentation results\n");
        return;
    }

    if (count == 0) {
        printf("No instrumentation data collected\n");
        return;
    }

    /* Sort by total time */
    qsort(entries, count, sizeof(instrument_entry_t), compare_by_total_time);

    printf("\n=== Instrumentation Profiling Report ===\n");
    printf("%-40s %10s %15s %15s %15s\n",
           "Function", "Calls", "Total (ms)", "Avg (μs)", "Max (μs)");
    printf("%.80s\n", "--------------------------------------------------------------------------------");

    for (int i = 0; i < count; i++) {
        instrument_entry_t *e = &entries[i];
        double total_ms = e->total_time_ns / 1000000.0;
        double avg_us = e->avg_time_ns / 1000.0;
        double max_us = e->max_time_ns / 1000.0;

        printf("%-40s %10lu %15.3f %15.3f %15.3f\n",
               e->function_name,
               e->call_count,
               total_ms,
               avg_us,
               max_us);
    }

    printf("%.80s\n", "--------------------------------------------------------------------------------");
    printf("Total functions profiled: %d\n", count);

    free(entries);
}

/* Cleanup profiler */
void profiler_destroy(profiler_t *prof) {
    if (!prof) return;

    pthread_mutex_lock(&profiler_mutex);

    if (prof->hash_table) {
        for (int i = 0; i < prof->hash_table_size; i++) {
            instrument_node_t *node = prof->hash_table[i];
            while (node) {
                instrument_node_t *next = node->next;
                free(node->function_name);
                free(node);
                node = next;
            }
        }
        free(prof->hash_table);
    }

    if (g_profiler == prof) {
        g_profiler = NULL;
    }

    pthread_mutex_unlock(&profiler_mutex);

    free(prof);
}
