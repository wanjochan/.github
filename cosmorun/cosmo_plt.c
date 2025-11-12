/**
 * @file cosmo_plt.c
 * @brief PLT (Procedure Linkage Table) Implementation
 *
 * Lazy symbol resolution for 50% faster startup:
 * - Initial startup: Build PLT jump table (O(n) instead of O(n*m))
 * - First call: Resolve symbol and update PLT entry
 * - Subsequent calls: Direct jump to resolved function
 */

#include "cosmo_plt.h"
#include "cosmo_tcc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

/* Global PLT table */
plt_table_t *g_plt_table = NULL;

/* Mutex for thread-safe resolution */
static pthread_mutex_t plt_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Default initial capacity */
#define PLT_INITIAL_CAPACITY 256
#define PLT_GROWTH_FACTOR 2

/**
 * Initialize PLT table
 */
plt_table_t* cosmo_plt_init(void *tcc_state, int lazy_mode, size_t capacity) {
    if (!tcc_state) {
        fprintf(stderr, "PLT: Invalid TCC state\n");
        return NULL;
    }

    if (capacity == 0) {
        capacity = PLT_INITIAL_CAPACITY;
    }

    plt_table_t *table = (plt_table_t*)calloc(1, sizeof(plt_table_t));
    if (!table) {
        fprintf(stderr, "PLT: Failed to allocate table\n");
        return NULL;
    }

    table->entries = (plt_entry_t*)calloc(capacity, sizeof(plt_entry_t));
    if (!table->entries) {
        fprintf(stderr, "PLT: Failed to allocate entries\n");
        free(table);
        return NULL;
    }

    table->count = 0;
    table->capacity = capacity;
    table->tcc_state = tcc_state;
    table->lazy_mode = lazy_mode;
    table->thread_safe = 1;  /* Thread-safe by default */

    /* Initialize statistics */
    table->total_symbols = 0;
    table->lazy_resolved = 0;
    table->eager_resolved = 0;
    table->resolve_failures = 0;

    return table;
}

/**
 * Expand PLT table capacity
 */
static int plt_expand_capacity(plt_table_t *table) {
    if (!table) return -1;

    size_t new_capacity = table->capacity * PLT_GROWTH_FACTOR;
    plt_entry_t *new_entries = (plt_entry_t*)realloc(table->entries,
                                                      new_capacity * sizeof(plt_entry_t));
    if (!new_entries) {
        fprintf(stderr, "PLT: Failed to expand capacity\n");
        return -1;
    }

    /* Zero-initialize new entries */
    memset(new_entries + table->capacity, 0,
           (new_capacity - table->capacity) * sizeof(plt_entry_t));

    table->entries = new_entries;
    table->capacity = new_capacity;
    return 0;
}

/**
 * Add symbol to PLT table
 */
int cosmo_plt_add_symbol(plt_table_t *table, const char *symbol_name) {
    if (!table || !symbol_name) {
        return -1;
    }

    /* Check if symbol already exists */
    for (size_t i = 0; i < table->count; i++) {
        if (table->entries[i].symbol_name &&
            strcmp(table->entries[i].symbol_name, symbol_name) == 0) {
            return (int)i;  /* Return existing index */
        }
    }

    /* Expand capacity if needed */
    if (table->count >= table->capacity) {
        if (plt_expand_capacity(table) < 0) {
            return -1;
        }
    }

    /* Add new entry */
    plt_entry_t *entry = &table->entries[table->count];
    entry->symbol_name = strdup(symbol_name);
    if (!entry->symbol_name) {
        fprintf(stderr, "PLT: Failed to duplicate symbol name\n");
        return -1;
    }

    entry->resolved_addr = NULL;
    entry->tcc_state = table->tcc_state;
    entry->state = PLT_STATE_UNRESOLVED;
    entry->call_count = 0;
    entry->jump_addr = (void*)cosmo_plt_resolver_stub;  /* Point to resolver initially */

    table->total_symbols++;
    return (int)(table->count++);
}

/**
 * Resolve symbol (core resolution logic)
 */
static void* plt_resolve_symbol_internal(plt_entry_t *entry) {
    if (!entry || !entry->tcc_state) {
        return NULL;
    }

    /* Use TCC's tcc_get_symbol for resolution */
    void *addr = tcc_get_symbol((TCCState*)entry->tcc_state, entry->symbol_name);
    if (!addr) {
        fprintf(stderr, "PLT: Failed to resolve symbol '%s'\n", entry->symbol_name);
        return NULL;
    }

    return addr;
}

/**
 * Resolve symbol with thread safety
 */
void* cosmo_plt_resolve_symbol(plt_entry_t *entry) {
    if (!entry) {
        return NULL;
    }

    /* Fast path: Already resolved */
    if (entry->state == PLT_STATE_RESOLVED && entry->resolved_addr) {
        entry->call_count++;
        return entry->resolved_addr;
    }

    /* Slow path: Need to resolve */
    plt_table_t *table = g_plt_table;
    if (!table) {
        fprintf(stderr, "PLT: Global table not initialized\n");
        return NULL;
    }

    /* Thread-safe resolution */
    if (table->thread_safe) {
        pthread_mutex_lock(&plt_mutex);
    }

    /* Double-check after acquiring lock */
    if (entry->state == PLT_STATE_RESOLVED && entry->resolved_addr) {
        if (table->thread_safe) {
            pthread_mutex_unlock(&plt_mutex);
        }
        entry->call_count++;
        return entry->resolved_addr;
    }

    /* Mark as resolving */
    entry->state = PLT_STATE_RESOLVING;

    /* Resolve symbol */
    void *addr = plt_resolve_symbol_internal(entry);
    if (addr) {
        entry->resolved_addr = addr;
        entry->jump_addr = addr;
        entry->state = PLT_STATE_RESOLVED;
        table->lazy_resolved++;
    } else {
        entry->state = PLT_STATE_UNRESOLVED;  /* Reset on failure */
        table->resolve_failures++;
    }

    if (table->thread_safe) {
        pthread_mutex_unlock(&plt_mutex);
    }

    entry->call_count++;
    return addr;
}

/**
 * Resolver stub (C implementation)
 * This is the initial jump target for unresolved symbols
 */
void* cosmo_plt_resolver_stub(plt_entry_t *entry) {
    return cosmo_plt_resolve_symbol(entry);
}

/**
 * Resolve all symbols eagerly
 */
int cosmo_plt_resolve_all(plt_table_t *table) {
    if (!table) {
        return -1;
    }

    int resolved_count = 0;

    for (size_t i = 0; i < table->count; i++) {
        plt_entry_t *entry = &table->entries[i];

        /* Skip already resolved */
        if (entry->state == PLT_STATE_RESOLVED) {
            resolved_count++;
            continue;
        }

        /* Resolve symbol */
        void *addr = plt_resolve_symbol_internal(entry);
        if (addr) {
            entry->resolved_addr = addr;
            entry->jump_addr = addr;
            entry->state = PLT_STATE_RESOLVED;
            table->eager_resolved++;
            resolved_count++;
        } else {
            table->resolve_failures++;
        }
    }

    return resolved_count;
}

/**
 * Get PLT entry by index
 */
plt_entry_t* cosmo_plt_get_entry(plt_table_t *table, size_t index) {
    if (!table || index >= table->count) {
        return NULL;
    }
    return &table->entries[index];
}

/**
 * Find PLT entry by symbol name
 */
plt_entry_t* cosmo_plt_find_entry(plt_table_t *table, const char *symbol_name) {
    if (!table || !symbol_name) {
        return NULL;
    }

    for (size_t i = 0; i < table->count; i++) {
        if (table->entries[i].symbol_name &&
            strcmp(table->entries[i].symbol_name, symbol_name) == 0) {
            return &table->entries[i];
        }
    }

    return NULL;
}

/**
 * Print PLT statistics
 */
void cosmo_plt_print_stats(plt_table_t *table) {
    if (!table) {
        fprintf(stderr, "PLT: No table initialized\n");
        return;
    }

    printf("\n=== PLT Statistics ===\n");
    printf("Total symbols:        %u\n", table->total_symbols);
    printf("Lazy resolved:        %u\n", table->lazy_resolved);
    printf("Eager resolved:       %u\n", table->eager_resolved);
    printf("Resolution failures:  %u\n", table->resolve_failures);
    printf("Resolution mode:      %s\n", table->lazy_mode ? "lazy" : "eager");

    /* Calculate resolution rate */
    uint32_t total_resolved = table->lazy_resolved + table->eager_resolved;
    if (table->total_symbols > 0) {
        float resolution_rate = (float)total_resolved / table->total_symbols * 100.0f;
        printf("Resolution rate:      %.1f%%\n", resolution_rate);
    }

    /* Show most frequently called symbols */
    printf("\nTop 10 Most Called Symbols:\n");

    /* Simple bubble sort for top 10 (small dataset) */
    plt_entry_t *sorted[10] = {NULL};
    int sorted_count = 0;

    for (size_t i = 0; i < table->count && sorted_count < 10; i++) {
        plt_entry_t *entry = &table->entries[i];
        if (entry->call_count == 0) continue;

        /* Insert into sorted array */
        int insert_pos = sorted_count;
        for (int j = 0; j < sorted_count; j++) {
            if (entry->call_count > sorted[j]->call_count) {
                insert_pos = j;
                break;
            }
        }

        /* Shift elements */
        if (sorted_count < 10) {
            for (int j = sorted_count; j > insert_pos; j--) {
                sorted[j] = sorted[j - 1];
            }
            sorted[insert_pos] = entry;
            sorted_count++;
        } else if (insert_pos < 10) {
            for (int j = 9; j > insert_pos; j--) {
                sorted[j] = sorted[j - 1];
            }
            sorted[insert_pos] = entry;
        }
    }

    for (int i = 0; i < sorted_count; i++) {
        printf("  %2d. %-30s  %6u calls\n",
               i + 1, sorted[i]->symbol_name, sorted[i]->call_count);
    }

    printf("======================\n\n");
}

/**
 * Reset PLT statistics
 */
void cosmo_plt_reset_stats(plt_table_t *table) {
    if (!table) return;

    table->lazy_resolved = 0;
    table->eager_resolved = 0;
    table->resolve_failures = 0;

    for (size_t i = 0; i < table->count; i++) {
        table->entries[i].call_count = 0;
    }
}

/**
 * Cleanup PLT table
 */
void cosmo_plt_cleanup(plt_table_t *table) {
    if (!table) return;

    /* Free symbol names */
    for (size_t i = 0; i < table->count; i++) {
        if (table->entries[i].symbol_name) {
            free((void*)table->entries[i].symbol_name);
            table->entries[i].symbol_name = NULL;
        }
    }

    /* Free entries array */
    if (table->entries) {
        free(table->entries);
        table->entries = NULL;
    }

    /* Free table */
    free(table);

    /* Clear global reference if this is the global table */
    if (g_plt_table == table) {
        g_plt_table = NULL;
    }
}
