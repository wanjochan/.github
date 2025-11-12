/**
 * @file cosmo_parallel_link.h
 * @brief Parallel linking API for cosmorun
 */

#ifndef COSMO_PARALLEL_LINK_H
#define COSMO_PARALLEL_LINK_H

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations - do not use typedef to avoid conflicts */
struct ObjectFile;
struct MergedSection;

/**
 * Configure parallel linking
 * @param enabled 1 to enable, 0 to disable
 * @param thread_count Number of threads (0 = auto-detect)
 */
void cosmo_parallel_link_config(int enabled, int thread_count);

/**
 * Parse object files in parallel
 * @param objects Array of object file paths
 * @param count Number of objects
 * @param obj_files Output array (caller must allocate)
 * @return 0 on success, -1 on error
 */
int parallel_parse_objects(const char **objects, int count, struct ObjectFile **obj_files);

/**
 * Apply relocations in parallel
 * @param reloc_batch Array of relocations to apply
 * @param count Number of relocations
 * @param symtab Symbol table (read-only)
 * @param section_hash Section hash table (read-only)
 * @return Number of failed relocations
 */
int parallel_apply_relocations(void *reloc_batch, int count,
                              void *symtab, void *section_hash);

/* Timer utilities for benchmarking */
typedef struct {
    struct timespec start;
    struct timespec end;
    double elapsed_sec;
} ParallelTimer;

void parallel_timer_start(ParallelTimer *timer);
void parallel_timer_stop(ParallelTimer *timer);
double parallel_timer_elapsed(const ParallelTimer *timer);

#ifdef __cplusplus
}
#endif

#endif /* COSMO_PARALLEL_LINK_H */
