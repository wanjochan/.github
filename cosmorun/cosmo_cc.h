/**
 * @file cosmo_cc.h
 * @brief C Compiler Toolchain - AR, Linker, and Binary Utilities
 *
 * Provides complete toolchain functionality for CosmoRun:
 * - AR: Static library archiver (create/extract/list .a files)
 * - Linker: Standalone linker mode (.o -> executable)
 * - nm: Symbol table listing
 * - objdump: Disassembly and section information
 * - strip: Symbol removal utility
 */

#ifndef COSMO_CC_H
#define COSMO_CC_H

#ifdef __cplusplus
extern "C" {
#endif

/* ========== AR Archive Tool ========== */

/**
 * Create or update static library archive (.a file)
 * @param archive Output .a file path
 * @param objects Array of .o file paths
 * @param count Number of objects
 * @param verbose Enable verbose output
 * @return 0 on success, -1 on error
 */
int cosmo_ar_create(const char *archive, const char **objects, int count, int verbose);

/**
 * Extract members from archive
 * @param archive Input .a file path
 * @param member Member name to extract (NULL for all)
 * @param verbose Enable verbose output
 * @return 0 on success, -1 on error
 */
int cosmo_ar_extract(const char *archive, const char *member, int verbose);

/**
 * List archive contents
 * @param archive Input .a file path
 * @param verbose Enable verbose output (show sizes, dates)
 * @return 0 on success, -1 on error
 */
int cosmo_ar_list(const char *archive, int verbose);

/**
 * Delete member from archive
 * @param archive Archive .a file path
 * @param member Member name to delete
 * @return 0 on success, -1 on error
 */
int cosmo_ar_delete(const char *archive, const char *member);


/* ========== Standalone Linker ========== */

/**
 * Libc backend selection
 */
typedef enum {
    LIBC_COSMO,    /* Cosmopolitan libc (default) */
    LIBC_SYSTEM,   /* System libc (dynamic linking) */
    LIBC_MINI      /* Custom minimal libc */
} LibcBackend;

/**
 * Parse --libc=TYPE argument
 * @param arg The argument string (e.g., "cosmo", "system", "mini")
 * @return Parsed LibcBackend, or -1 on error
 */
int parse_libc_option(const char *arg);

/**
 * Link object files into executable
 * @param objects Array of .o file paths
 * @param count Number of objects
 * @param output Output executable path
 * @param lib_paths Array of library search paths (-L)
 * @param lib_count Number of library paths
 * @param libs Array of libraries to link (-l)
 * @param libs_count Number of libraries
 * @param libc_backend Libc backend to use (LIBC_COSMO, LIBC_SYSTEM, LIBC_MINI)
 * @param gc_sections Enable dead code elimination (1 = on, 0 = off)
 * @return 0 on success, -1 on error
 */
int cosmo_link(const char **objects, int count, const char *output,
               const char **lib_paths, int lib_count,
               const char **libs, int libs_count,
               LibcBackend libc_backend, int gc_sections);

/**
 * Set linker verbosity level
 * @param level 0=quiet, 1=default, 2=verbose, 3=very verbose
 */
void cosmo_linker_set_verbosity(int level);

/**
 * Enable symbol table dump (--dump-symbols)
 * @param enable 1 to enable, 0 to disable
 */
void cosmo_linker_set_dump_symbols(int enable);

/**
 * Enable relocation dump (--dump-relocations)
 * @param enable 1 to enable, 0 to disable
 */
void cosmo_linker_set_dump_relocations(int enable);

/**
 * Enable symbol resolution tracing (--trace-resolve)
 * @param enable 1 to enable, 0 to disable
 */
void cosmo_linker_set_trace_resolve(int enable);


/* ========== nm - Symbol Table Tool ========== */

/* nm output format flags */
#define NM_FORMAT_BSD    0  /* Default BSD format */
#define NM_FORMAT_POSIX  1  /* POSIX format */
#define NM_FORMAT_SYSV   2  /* System V format */

/* nm filter flags */
#define NM_FILTER_UNDEF  (1 << 0)  /* Show only undefined symbols */
#define NM_FILTER_EXTERN (1 << 1)  /* Show only external symbols */
#define NM_FILTER_DEBUG  (1 << 2)  /* Include debug symbols */

/**
 * List symbols from object file or executable
 * @param file Input file path (.o, .a, or executable)
 * @param format Output format (NM_FORMAT_*)
 * @param flags Filter flags (NM_FILTER_*)
 * @return 0 on success, -1 on error
 */
int cosmo_nm(const char *file, int format, int flags);


/* ========== objdump - Object File Disassembler ========== */

/* objdump display flags */
#define OBJDUMP_HEADERS     (1 << 0)  /* Display section headers */
#define OBJDUMP_DISASM      (1 << 1)  /* Disassemble code sections */
#define OBJDUMP_SYMBOLS     (1 << 2)  /* Display symbol table */
#define OBJDUMP_RELOC       (1 << 3)  /* Display relocations */
#define OBJDUMP_FULL_CONTENTS (1 << 4) /* Display full section contents */

/**
 * Disassemble and display object file information
 * @param file Input file path
 * @param flags Display flags (OBJDUMP_*)
 * @return 0 on success, -1 on error
 */
int cosmo_objdump(const char *file, int flags);


/* ========== strip - Symbol Removal Tool ========== */

/* strip options */
#define STRIP_ALL           (1 << 0)  /* Remove all symbols */
#define STRIP_DEBUG         (1 << 1)  /* Remove debug symbols only */
#define STRIP_UNNEEDED      (1 << 2)  /* Remove symbols not needed for relocation */

/**
 * Remove symbols from object file or executable
 * @param input Input file path
 * @param output Output file path (NULL for in-place)
 * @param flags Strip options (STRIP_*)
 * @return 0 on success, -1 on error
 */
int cosmo_strip(const char *input, const char *output, int flags);


#ifdef __cplusplus
}
#endif

#endif /* COSMO_CC_H */
