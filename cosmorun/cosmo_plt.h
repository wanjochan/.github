/**
 * @file cosmo_plt.h
 * @brief PLT (Procedure Linkage Table) for Lazy Symbol Resolution
 *
 * Implements lazy symbol resolution similar to ELF PLT mechanism:
 * - Build jump table with resolver stubs on startup (fast O(n))
 * - On first call: resolve symbol and update PLT entry
 * - Subsequent calls: direct jump to resolved function
 *
 * Expected speedup: 50% faster startup for programs with many symbols
 */

#ifndef COSMO_PLT_H
#define COSMO_PLT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* PLT entry states */
typedef enum {
    PLT_STATE_UNRESOLVED = 0,  /* Not yet resolved (initial state) */
    PLT_STATE_RESOLVING  = 1,  /* Being resolved (lock held) */
    PLT_STATE_RESOLVED   = 2   /* Fully resolved */
} plt_state_t;

/* PLT entry structure (one per symbol) */
typedef struct plt_entry {
    const char *symbol_name;      /* Symbol name to resolve */
    void *resolved_addr;          /* Resolved function address (NULL if unresolved) */
    void *tcc_state;              /* TCC state for symbol lookup */
    volatile int state;           /* PLT entry state (plt_state_t) */
    uint32_t call_count;          /* Number of times called (for stats) */
    void *jump_addr;              /* Jump target (resolver stub or actual function) */
} plt_entry_t;

/* PLT table structure */
typedef struct plt_table {
    plt_entry_t *entries;         /* Array of PLT entries */
    size_t count;                 /* Number of entries */
    size_t capacity;              /* Allocated capacity */
    void *tcc_state;              /* TCC state for symbol resolution */

    /* Statistics */
    uint32_t total_symbols;       /* Total number of symbols */
    uint32_t lazy_resolved;       /* Number of lazy-resolved symbols */
    uint32_t eager_resolved;      /* Number of eager-resolved symbols */
    uint32_t resolve_failures;    /* Number of resolution failures */

    /* Configuration */
    int lazy_mode;                /* 1 = lazy resolution, 0 = eager resolution */
    int thread_safe;              /* 1 = use locks, 0 = no locks */
} plt_table_t;

/* Global PLT table (one per TCC state) */
extern plt_table_t *g_plt_table;

/**
 * Initialize PLT table
 * @param tcc_state TCC state for symbol resolution
 * @param lazy_mode 1 for lazy resolution, 0 for eager
 * @param capacity Initial capacity (number of symbols expected)
 * @return Initialized PLT table, or NULL on error
 */
plt_table_t* cosmo_plt_init(void *tcc_state, int lazy_mode, size_t capacity);

/**
 * Add symbol to PLT table
 * @param table PLT table
 * @param symbol_name Symbol name to add
 * @return PLT entry index, or -1 on error
 */
int cosmo_plt_add_symbol(plt_table_t *table, const char *symbol_name);

/**
 * Get resolved function address for symbol
 * This is the main entry point called by generated code
 * @param entry PLT entry
 * @return Resolved function address (or NULL on failure)
 */
void* cosmo_plt_resolve_symbol(plt_entry_t *entry);

/**
 * Resolve all symbols eagerly (for eager mode or warmup)
 * @param table PLT table
 * @return Number of successfully resolved symbols
 */
int cosmo_plt_resolve_all(plt_table_t *table);

/**
 * Print PLT statistics (for --plt-stats)
 * @param table PLT table
 */
void cosmo_plt_print_stats(plt_table_t *table);

/**
 * Cleanup PLT table
 * @param table PLT table to free
 */
void cosmo_plt_cleanup(plt_table_t *table);

/**
 * Reset PLT statistics
 * @param table PLT table
 */
void cosmo_plt_reset_stats(plt_table_t *table);

/**
 * Get PLT entry by index
 * @param table PLT table
 * @param index Entry index
 * @return PLT entry, or NULL if invalid
 */
plt_entry_t* cosmo_plt_get_entry(plt_table_t *table, size_t index);

/**
 * Get PLT entry by symbol name
 * @param table PLT table
 * @param symbol_name Symbol name
 * @return PLT entry, or NULL if not found
 */
plt_entry_t* cosmo_plt_find_entry(plt_table_t *table, const char *symbol_name);

/**
 * Resolver stub (assembly trampoline)
 * This is the initial jump target for unresolved symbols
 * It calls cosmo_plt_resolve_symbol() and updates the PLT entry
 */
void* cosmo_plt_resolver_stub(plt_entry_t *entry);

#ifdef __cplusplus
}
#endif

#endif /* COSMO_PLT_H */
