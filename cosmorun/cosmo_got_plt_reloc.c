/**
 * @file cosmo_got_plt_reloc.c
 * @brief GOT/PLT Table Generator Implementation
 *
 * Fixes relocation overflow by creating indirection tables
 */

#include "cosmo_got_plt_reloc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Constants */
#define OVERFLOW_INITIAL_CAPACITY 128
#define PLT_STUB_SIZE_X86_64 16
#define PLT_STUB_SIZE_ARM64 16
#define PC32_MAX_RANGE 0x7FFFFFFFL  /* ±2GB signed */

/* ========== Overflow List Management ========== */

OverflowList* init_overflow_list(int initial_capacity) {
    if (initial_capacity <= 0) {
        initial_capacity = OVERFLOW_INITIAL_CAPACITY;
    }

    OverflowList *list = (OverflowList*)calloc(1, sizeof(OverflowList));
    if (!list) {
        fprintf(stderr, "[got_plt] Failed to allocate overflow list\n");
        return NULL;
    }

    list->entries = (OverflowCandidate*)calloc(initial_capacity, sizeof(OverflowCandidate));
    if (!list->entries) {
        fprintf(stderr, "[got_plt] Failed to allocate overflow entries\n");
        free(list);
        return NULL;
    }

    list->count = 0;
    list->capacity = initial_capacity;
    return list;
}

int add_overflow_candidate(OverflowList *list, const char *symbol_name,
                          uint64_t symbol_addr, uint64_t reloc_offset,
                          uint32_t reloc_type, int64_t addend,
                          uint64_t source_addr, int64_t overflow_amount,
                          void *target_section) {
    if (!list || !symbol_name) {
        return -1;
    }

    /* Expand capacity if needed */
    if (list->count >= list->capacity) {
        int new_capacity = list->capacity * 2;
        OverflowCandidate *new_entries = (OverflowCandidate*)realloc(
            list->entries, new_capacity * sizeof(OverflowCandidate));
        if (!new_entries) {
            fprintf(stderr, "[got_plt] Failed to expand overflow list\n");
            return -1;
        }
        list->entries = new_entries;
        list->capacity = new_capacity;
    }

    /* Add new entry */
    OverflowCandidate *entry = &list->entries[list->count];
    strncpy(entry->symbol_name, symbol_name, sizeof(entry->symbol_name) - 1);
    entry->symbol_name[sizeof(entry->symbol_name) - 1] = '\0';
    entry->symbol_addr = symbol_addr;
    entry->reloc_offset = reloc_offset;
    entry->reloc_type = reloc_type;
    entry->addend = addend;
    entry->source_addr = source_addr;
    entry->overflow_amount = overflow_amount;
    entry->target_section = target_section;

    list->count++;
    return 0;
}

void free_overflow_list(OverflowList *list) {
    if (!list) return;
    free(list->entries);
    free(list);
}

/* ========== PLT Stub Generation ========== */

int generate_plt_stub_x86_64(uint8_t *stub_out, uint64_t stub_addr, uint64_t got_entry_addr) {
    if (!stub_out) return -1;

    /* Calculate PC-relative offset: GOT - (PLT + 6)
     * The +6 is because RIP points to next instruction after jmp */
    int64_t offset = (int64_t)got_entry_addr - (int64_t)(stub_addr + 6);

    /* Check if offset fits in signed 32-bit */
    if (offset < (int64_t)INT32_MIN || offset > (int64_t)INT32_MAX) {
        fprintf(stderr, "[got_plt] ERROR: GOT entry out of PC32 range from PLT stub\n");
        fprintf(stderr, "  PLT stub: 0x%lx, GOT entry: 0x%lx, offset: %ld\n",
                stub_addr, got_entry_addr, offset);
        return -1;
    }

    /* Generate: jmp [rip + offset]
     * Opcode: FF 25 [4-byte offset] */
    stub_out[0] = 0xFF;  /* JMP indirect */
    stub_out[1] = 0x25;  /* ModR/M: [rip + disp32] */
    *(int32_t*)(stub_out + 2) = (int32_t)offset;

    /* Padding with NOPs to 16-byte boundary */
    for (int i = 6; i < PLT_STUB_SIZE_X86_64; i++) {
        stub_out[i] = 0x90;  /* NOP */
    }

    return PLT_STUB_SIZE_X86_64;
}

int generate_plt_stub_arm64(uint8_t *stub_out, uint64_t stub_addr, uint64_t got_entry_addr) {
    if (!stub_out) return -1;

    /* ARM64 PLT stub:
     *   adrp x16, got_page           ; Load page address
     *   ldr  x16, [x16, #page_offset]; Load from GOT
     *   br   x16                     ; Branch to target
     */

    uint64_t page_pc = stub_addr & ~0xFFFULL;
    uint64_t page_target = got_entry_addr & ~0xFFFULL;
    int64_t page_offset = ((int64_t)page_target - (int64_t)page_pc) >> 12;

    if (page_offset < -(1LL << 20) || page_offset >= (1LL << 20)) {
        fprintf(stderr, "[got_plt] ERROR: GOT entry page out of range for ARM64 ADRP\n");
        return -1;
    }

    uint32_t lo = (uint32_t)(page_offset & 0x3) << 29;
    uint32_t hi = (uint32_t)((page_offset >> 2) & 0x7FFFF) << 5;
    uint32_t adrp = 0x90000010 | lo | hi;  /* adrp x16, #page_offset */

    uint32_t ldr_offset = (uint32_t)(got_entry_addr & 0xFFF) >> 3;
    uint32_t ldr = 0xF9400210 | (ldr_offset << 10);  /* ldr x16, [x16, #offset] */

    uint32_t br = 0xD61F0200;  /* br x16 */

    memcpy(stub_out + 0, &adrp, 4);
    memcpy(stub_out + 4, &ldr, 4);
    memcpy(stub_out + 8, &br, 4);
    memset(stub_out + 12, 0, 4);  /* Padding */

    return PLT_STUB_SIZE_ARM64;
}

/* ========== GOT/PLT Table Creation ========== */

GotPltTable* create_got_plt_table(OverflowList *overflows, uint64_t code_end, GotPltArch arch) {
    if (!overflows || overflows->count == 0) {
        return NULL;  /* No overflows - no table needed */
    }

    GotPltTable *table = (GotPltTable*)calloc(1, sizeof(GotPltTable));
    if (!table) {
        fprintf(stderr, "[got_plt] Failed to allocate GOT/PLT table\n");
        return NULL;
    }

    table->arch = arch;

    /* Count unique symbols (avoid duplicates) */
    int unique_count = 0;
    char **unique_symbols = (char**)calloc(overflows->count, sizeof(char*));
    if (!unique_symbols) {
        free(table);
        return NULL;
    }

    for (int i = 0; i < overflows->count; i++) {
        const char *sym = overflows->entries[i].symbol_name;
        int found = 0;
        for (int j = 0; j < unique_count; j++) {
            if (strcmp(unique_symbols[j], sym) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            unique_symbols[unique_count++] = (char*)sym;
        }
    }

    table->got_count = unique_count;
    table->plt_count = unique_count;

    /* Allocate GOT entries */
    table->got_entries = (GotEntry*)calloc(unique_count, sizeof(GotEntry));
    table->got_data = (uint64_t*)calloc(unique_count, sizeof(uint64_t));
    if (!table->got_entries || !table->got_data) {
        fprintf(stderr, "[got_plt] Failed to allocate GOT entries\n");
        free(unique_symbols);
        free_got_plt_table(table);
        return NULL;
    }

    /* Allocate PLT entries */
    int stub_size = (arch == ARCH_X86_64) ? PLT_STUB_SIZE_X86_64 : PLT_STUB_SIZE_ARM64;
    table->plt_entries = (PltEntry*)calloc(unique_count, sizeof(PltEntry));
    table->plt_data = (uint8_t*)calloc(unique_count, stub_size);
    if (!table->plt_entries || !table->plt_data) {
        fprintf(stderr, "[got_plt] Failed to allocate PLT entries\n");
        free(unique_symbols);
        free_got_plt_table(table);
        return NULL;
    }

    /* Set section addresses:
     * GOT immediately after code section (page-aligned)
     * PLT immediately after GOT */
    uint64_t page_size = 0x1000;
    table->got_base = (code_end + page_size - 1) & ~(page_size - 1);
    table->got_size = unique_count * sizeof(uint64_t);
    table->plt_base = table->got_base + table->got_size;
    table->plt_size = unique_count * stub_size;

    /* Populate GOT and PLT entries */
    for (int i = 0; i < unique_count; i++) {
        const char *sym_name = unique_symbols[i];

        /* Find corresponding overflow entry to get symbol address */
        uint64_t sym_addr = 0;
        for (int j = 0; j < overflows->count; j++) {
            if (strcmp(overflows->entries[j].symbol_name, sym_name) == 0) {
                sym_addr = overflows->entries[j].symbol_addr;
                break;
            }
        }

        /* GOT entry */
        strncpy(table->got_entries[i].symbol_name, sym_name, sizeof(table->got_entries[i].symbol_name) - 1);
        table->got_entries[i].symbol_addr = sym_addr;
        table->got_entries[i].got_index = i;
        table->got_data[i] = sym_addr;  /* GOT contains absolute address */

        /* PLT entry */
        strncpy(table->plt_entries[i].symbol_name, sym_name, sizeof(table->plt_entries[i].symbol_name) - 1);
        table->plt_entries[i].plt_addr = table->plt_base + i * stub_size;
        table->plt_entries[i].got_index = i;
        table->plt_entries[i].stub_size = stub_size;

        /* Generate PLT stub */
        uint64_t stub_addr = table->plt_entries[i].plt_addr;
        uint64_t got_entry_addr = table->got_base + i * sizeof(uint64_t);
        uint8_t *stub_code = table->plt_data + i * stub_size;

        int gen_size = 0;
        if (arch == ARCH_X86_64) {
            gen_size = generate_plt_stub_x86_64(stub_code, stub_addr, got_entry_addr);
        } else if (arch == ARCH_ARM64) {
            gen_size = generate_plt_stub_arm64(stub_code, stub_addr, got_entry_addr);
        }

        if (gen_size < 0) {
            fprintf(stderr, "[got_plt] Failed to generate PLT stub for %s\n", sym_name);
            free(unique_symbols);
            free_got_plt_table(table);
            return NULL;
        }

        memcpy(table->plt_entries[i].stub_code, stub_code, stub_size);
    }

    free(unique_symbols);

    printf("[got_plt] Created GOT/PLT table: %d entries\n", unique_count);
    printf("  GOT: 0x%lx - 0x%lx (%zu bytes)\n",
           table->got_base, table->got_base + table->got_size, table->got_size);
    printf("  PLT: 0x%lx - 0x%lx (%zu bytes)\n",
           table->plt_base, table->plt_base + table->plt_size, table->plt_size);

    return table;
}

int find_plt_entry(GotPltTable *table, const char *symbol_name) {
    if (!table || !symbol_name) return -1;

    for (int i = 0; i < table->plt_count; i++) {
        if (strcmp(table->plt_entries[i].symbol_name, symbol_name) == 0) {
            return i;
        }
    }

    return -1;  /* Not found */
}

uint64_t get_plt_address(GotPltTable *table, const char *symbol_name) {
    int idx = find_plt_entry(table, symbol_name);
    if (idx < 0) return 0;
    return table->plt_entries[idx].plt_addr;
}

void free_got_plt_table(GotPltTable *table) {
    if (!table) return;

    free(table->got_entries);
    free(table->got_data);
    free(table->plt_entries);
    free(table->plt_data);
    free(table);
}

void print_got_plt_stats(GotPltTable *table) {
    if (!table) return;

    printf("\n=== GOT/PLT Statistics ===\n");
    printf("Architecture: %s\n",
           table->arch == ARCH_X86_64 ? "x86-64" :
           table->arch == ARCH_ARM64 ? "ARM64" : "Unknown");
    printf("GOT entries: %d\n", table->got_count);
    printf("PLT stubs: %d\n", table->plt_count);
    printf("Total redirects: %d\n", table->total_redirects);
    printf("  PC32 redirects: %d\n", table->pc32_redirects);
    printf("  PLT32 redirects: %d\n", table->plt32_redirects);
    printf("Memory overhead: %zu bytes\n", table->got_size + table->plt_size);
    printf("========================\n\n");
}

/* ========== Validation ========== */

int is_within_pc32_range(uint64_t source, uint64_t target) {
    int64_t offset = (int64_t)target - (int64_t)source;
    return (offset >= (int64_t)INT32_MIN && offset <= (int64_t)INT32_MAX);
}

int validate_got_plt_table(GotPltTable *table, uint64_t code_base, size_t code_size) {
    if (!table) return -1;

    /* Check GOT is reachable from code section */
    uint64_t code_end = code_base + code_size;
    if (!is_within_pc32_range(code_base, table->got_base) ||
        !is_within_pc32_range(code_end, table->got_base + table->got_size)) {
        fprintf(stderr, "[got_plt] ERROR: GOT not reachable from code section\n");
        return -1;
    }

    /* Check PLT is reachable from code section */
    if (!is_within_pc32_range(code_base, table->plt_base) ||
        !is_within_pc32_range(code_end, table->plt_base + table->plt_size)) {
        fprintf(stderr, "[got_plt] ERROR: PLT not reachable from code section\n");
        return -1;
    }

    /* Validate each PLT stub can reach its GOT entry */
    for (int i = 0; i < table->plt_count; i++) {
        uint64_t plt_addr = table->plt_entries[i].plt_addr;
        uint64_t got_addr = table->got_base + i * sizeof(uint64_t);

        if (!is_within_pc32_range(plt_addr, got_addr)) {
            fprintf(stderr, "[got_plt] ERROR: PLT stub %d cannot reach GOT entry\n", i);
            fprintf(stderr, "  PLT: 0x%lx, GOT: 0x%lx\n", plt_addr, got_addr);
            return -1;
        }
    }

    return 0;  /* All checks passed */
}
