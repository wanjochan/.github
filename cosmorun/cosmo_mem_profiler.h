/* Memory profiler for cosmorun runtime
 * Tracks allocations, peak memory usage, and size distribution
 */

#ifndef COSMO_MEM_PROFILER_H
#define COSMO_MEM_PROFILER_H

#include <stddef.h>

/* Initialize memory profiler */
void mem_profiler_init(void);

/* Shutdown and free profiler resources */
void mem_profiler_shutdown(void);

/* Tracked malloc wrapper */
void* mem_profiler_malloc(size_t size);

/* Tracked malloc with module name for per-module tracking */
void* mem_profiler_malloc_module(size_t size, const char *module);

/* Tracked free wrapper */
void mem_profiler_free(void *ptr);

/* Print memory usage statistics */
void mem_profiler_report(void);

/* Query statistics programmatically */
size_t mem_profiler_get_total_allocated(void);
size_t mem_profiler_get_peak_memory(void);
size_t mem_profiler_get_allocation_count(void);

#endif /* COSMO_MEM_PROFILER_H */
