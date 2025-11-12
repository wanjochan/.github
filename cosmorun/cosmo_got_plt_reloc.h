/**
 * @file cosmo_got_plt_reloc.h
 * @brief GOT/PLT Table Generator for Relocation Overflow Handling
 *
 * Purpose: Fix relocation overflow errors by redirecting distant symbols
 *          through GOT (Global Offset Table) and PLT (Procedure Linkage Table)
 *
 * Problem: PC-relative 32-bit relocations (R_X86_64_PC32/PLT32) overflow
 *          when target is >2GB away from source location
 *
 * Solution:
 *  - GOT: Array of 64-bit absolute addresses (for data references)
 *  - PLT: Array of indirect jump stubs (for function calls)
 *  - Both placed near code section (within ±2GB range)
 *
 * Example:
 *  Before (overflow):
 *    call func_at_2.5GB_away    ; PC32 overflow
 *
 *  After (redirected through PLT):
 *    call plt_stub              ; PC32 to PLT (< 2GB)
 *    plt_stub:
 *      jmp [got_entry]          ; Indirect jump through GOT
 *    got_entry:
 *      .quad func_at_2.5GB_away ; Absolute 64-bit address
 *
 * Usage:
 *  1. Detect overflows during first relocation pass
 *  2. Create GOT/PLT table with overflow candidates
 *  3. Rewrite overflow relocations to target PLT stubs
 *  4. Embed GOT/PLT sections in final binary
 */

#ifndef COSMO_GOT_PLT_RELOC_H
#define COSMO_GOT_PLT_RELOC_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Architecture types */
typedef enum {
    ARCH_UNKNOWN = 0,
    ARCH_X86_64 = 1,
    ARCH_ARM64 = 2
} GotPltArch;

/* Overflow relocation candidate */
typedef struct {
    char symbol_name[256];      /* Symbol name */
    uint64_t symbol_addr;       /* Target symbol address (S) */
    uint64_t reloc_offset;      /* Relocation offset in section */
    uint32_t reloc_type;        /* R_X86_64_PC32, R_X86_64_PLT32, etc. */
    int64_t addend;             /* Relocation addend (A) */
    uint64_t source_addr;       /* Source address (P) */
    int64_t overflow_amount;    /* How much overflow (for diagnostics) */
    void *target_section;       /* Section containing this relocation */
} OverflowCandidate;

/* Overflow candidate list */
typedef struct {
    OverflowCandidate *entries;
    int count;
    int capacity;
} OverflowList;

/* GOT entry (64-bit absolute address) */
typedef struct {
    char symbol_name[256];
    uint64_t symbol_addr;       /* Absolute address */
    int got_index;              /* Index in GOT array */
} GotEntry;

/* PLT stub (architecture-specific jump code) */
typedef struct {
    char symbol_name[256];
    uint64_t plt_addr;          /* Address of PLT stub */
    int got_index;              /* Associated GOT entry index */
    uint8_t stub_code[16];      /* PLT stub machine code (max 16 bytes) */
    int stub_size;              /* Actual stub size */
} PltEntry;

/* Complete GOT/PLT table */
typedef struct {
    /* Architecture */
    GotPltArch arch;

    /* GOT section */
    uint64_t got_base;          /* GOT section base address */
    GotEntry *got_entries;      /* Array of GOT entries */
    int got_count;

    /* PLT section */
    uint64_t plt_base;          /* PLT section base address */
    PltEntry *plt_entries;      /* Array of PLT stubs */
    int plt_count;

    /* Raw data for embedding */
    uint64_t *got_data;         /* Raw GOT data (64-bit addresses) */
    uint8_t *plt_data;          /* Raw PLT code */
    size_t got_size;            /* GOT section size (bytes) */
    size_t plt_size;            /* PLT section size (bytes) */

    /* Statistics */
    int total_redirects;        /* Number of redirected relocations */
    int pc32_redirects;         /* R_X86_64_PC32 redirects */
    int plt32_redirects;        /* R_X86_64_PLT32 redirects */
} GotPltTable;

/* ========== Core Functions ========== */

/**
 * Create GOT/PLT table from overflow candidates
 * @param overflows List of overflow relocations
 * @param code_end End address of code section (for placement)
 * @param arch Target architecture
 * @return Initialized GOT/PLT table, or NULL on error
 */
GotPltTable* create_got_plt_table(OverflowList *overflows, uint64_t code_end, GotPltArch arch);

/**
 * Find or create PLT entry for symbol
 * @param table GOT/PLT table
 * @param symbol_name Symbol to look up
 * @return PLT entry index, or -1 if not found
 */
int find_plt_entry(GotPltTable *table, const char *symbol_name);

/**
 * Get PLT stub address for symbol
 * @param table GOT/PLT table
 * @param symbol_name Symbol name
 * @return PLT stub address, or 0 if not found
 */
uint64_t get_plt_address(GotPltTable *table, const char *symbol_name);

/**
 * Free GOT/PLT table
 * @param table Table to free
 */
void free_got_plt_table(GotPltTable *table);

/**
 * Print GOT/PLT statistics
 * @param table GOT/PLT table
 */
void print_got_plt_stats(GotPltTable *table);

/* ========== Overflow Detection ========== */

/**
 * Initialize overflow list
 * @param initial_capacity Initial capacity (number of entries)
 * @return Initialized overflow list, or NULL on error
 */
OverflowList* init_overflow_list(int initial_capacity);

/**
 * Add overflow candidate to list
 * @param list Overflow list
 * @param symbol_name Symbol name
 * @param symbol_addr Target address (S)
 * @param reloc_offset Relocation offset
 * @param reloc_type Relocation type
 * @param addend Relocation addend (A)
 * @param source_addr Source address (P)
 * @param overflow_amount Overflow amount (bytes)
 * @param target_section Section pointer
 * @return 0 on success, -1 on error
 */
int add_overflow_candidate(OverflowList *list, const char *symbol_name,
                          uint64_t symbol_addr, uint64_t reloc_offset,
                          uint32_t reloc_type, int64_t addend,
                          uint64_t source_addr, int64_t overflow_amount,
                          void *target_section);

/**
 * Free overflow list
 * @param list List to free
 */
void free_overflow_list(OverflowList *list);

/* ========== Architecture-Specific Stub Generation ========== */

/**
 * Generate x86-64 PLT stub
 * Format: jmp [got_entry_addr]  ; FF 25 xx xx xx xx (6 bytes)
 *         padding (10 bytes to 16-byte boundary)
 *
 * @param stub_out Output buffer (min 16 bytes)
 * @param stub_addr Address of PLT stub itself
 * @param got_entry_addr Address of GOT entry
 * @return Stub size (bytes), or -1 on error
 */
int generate_plt_stub_x86_64(uint8_t *stub_out, uint64_t stub_addr, uint64_t got_entry_addr);

/**
 * Generate ARM64 PLT stub
 * Format: adrp x16, got_entry     ; Page-relative load
 *         ldr x16, [x16, #offset] ; Load from GOT
 *         br x16                   ; Jump to target
 *
 * @param stub_out Output buffer (min 16 bytes)
 * @param stub_addr Address of PLT stub itself
 * @param got_entry_addr Address of GOT entry
 * @return Stub size (bytes), or -1 on error
 */
int generate_plt_stub_arm64(uint8_t *stub_out, uint64_t stub_addr, uint64_t got_entry_addr);

/* ========== Validation ========== */

/**
 * Check if address is within PC32 range from source
 * @param source Source address (P)
 * @param target Target address (S)
 * @return 1 if within range, 0 if overflow
 */
int is_within_pc32_range(uint64_t source, uint64_t target);

/**
 * Validate GOT/PLT table integrity
 * @param table GOT/PLT table
 * @param code_base Code section base address
 * @param code_size Code section size
 * @return 0 if valid, -1 on error
 */
int validate_got_plt_table(GotPltTable *table, uint64_t code_base, size_t code_size);

#ifdef __cplusplus
}
#endif

#endif /* COSMO_GOT_PLT_RELOC_H */
