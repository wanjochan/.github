/* cosmo_elf_parser.h - ELF Parser for Dynamic Dependencies
 *
 * Purpose: Parse ELF files to extract dynamic linking information
 *
 * Features:
 * - Parse ELF64 and ELF32 formats
 * - Extract PT_DYNAMIC segment
 * - Parse DT_NEEDED entries (required shared libraries)
 * - Extract RPATH and RUNPATH
 * - Handle both executable and shared library formats
 */

#ifndef COSMO_ELF_PARSER_H
#define COSMO_ELF_PARSER_H

#include <stdint.h>
#include <stddef.h>
#include <elf.h>

/* Fallback definitions if not in system elf.h */
#ifndef PT_DYNAMIC
#define PT_DYNAMIC 2
#endif

#ifndef DT_NEEDED
#define DT_NEEDED 1
#endif

#ifndef DT_STRTAB
#define DT_STRTAB 5
#endif

#ifndef DT_RPATH
#define DT_RPATH 15
#endif

#ifndef DT_RUNPATH
#define DT_RUNPATH 29
#endif

/* ========== ELF Format Detection ========== */

typedef enum {
    ELF_CLASS_NONE = 0,
    ELF_CLASS_32 = 1,
    ELF_CLASS_64 = 2
} elf_class_t;

/**
 * Detect ELF file class (32-bit or 64-bit)
 *
 * @param data: File data (at least 16 bytes)
 * @param size: Size of file data
 * @return: ELF_CLASS_32, ELF_CLASS_64, or ELF_CLASS_NONE
 */
elf_class_t elf_detect_class(const uint8_t *data, size_t size);

/* ========== Dynamic Section Entry ========== */

typedef struct {
    int64_t tag;       /* Entry type (DT_NEEDED, DT_RPATH, etc.) */
    uint64_t value;    /* Entry value (string table offset, address, etc.) */
} elf_dynamic_entry_t;

/* ========== ELF Parser Context ========== */

typedef struct {
    const uint8_t *file_data;       /* Pointer to file data */
    size_t file_size;               /* Total file size */
    elf_class_t elf_class;          /* ELF class (32 or 64) */

    /* Dynamic section */
    elf_dynamic_entry_t *dynamic;   /* Dynamic section entries */
    int num_dynamic;                /* Number of dynamic entries */

    /* String table */
    const char *strtab;             /* Pointer to string table */
    size_t strtab_size;             /* String table size */

    int valid;                       /* Parser successfully initialized */
} elf_parser_t;

/* ========== Parser Lifecycle ========== */

/**
 * Initialize ELF parser
 *
 * @param parser: Parser context to initialize
 * @param data: Pointer to ELF file data
 * @param size: Size of file data
 * @return: 0 on success, -1 on error
 */
int elf_parser_init(elf_parser_t *parser, const uint8_t *data, size_t size);

/**
 * Free parser resources
 *
 * @param parser: Parser context to free
 */
void elf_parser_free(elf_parser_t *parser);

/* ========== Dynamic Section Parsing ========== */

/**
 * Parse PT_DYNAMIC segment
 *
 * @param parser: Initialized parser context
 * @return: 0 on success, -1 on error
 */
int elf_parse_dynamic(elf_parser_t *parser);

/**
 * Get needed libraries (DT_NEEDED entries)
 *
 * @param parser: Parser context with parsed dynamic section
 * @param libs: Output array of library names (caller must free)
 * @param num_libs: Output number of libraries
 * @return: 0 on success, -1 on error
 */
int elf_get_needed_libs(const elf_parser_t *parser, char ***libs, int *num_libs);

/**
 * Get RPATH (DT_RPATH entry)
 *
 * @param parser: Parser context with parsed dynamic section
 * @return: RPATH string or NULL if not present
 */
const char* elf_get_rpath(const elf_parser_t *parser);

/**
 * Get RUNPATH (DT_RUNPATH entry)
 *
 * @param parser: Parser context with parsed dynamic section
 * @return: RUNPATH string or NULL if not present
 */
const char* elf_get_runpath(const elf_parser_t *parser);

/* ========== Utility Functions ========== */

/**
 * Get string from dynamic string table
 *
 * @param parser: Parser context
 * @param offset: Offset into string table
 * @return: String pointer or NULL on error
 */
const char* elf_get_string(const elf_parser_t *parser, uint64_t offset);

/**
 * Check if file is a valid ELF
 *
 * @param data: File data
 * @param size: File size
 * @return: 1 if valid ELF, 0 otherwise
 */
int elf_is_valid(const uint8_t *data, size_t size);

#endif /* COSMO_ELF_PARSER_H */
