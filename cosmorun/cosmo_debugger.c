/* Unified debugger implementation: DWARF backtrace (R42-B) + hardware watchpoints (R42-C) */
#include "cosmo_debugger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <elf.h>

/* ptrace and struct user only available on x86-64 Linux */
#if defined(__x86_64__) && defined(__linux__)
#include <sys/ptrace.h>
#include <sys/user.h>
#define HAVE_HW_WATCHPOINTS 1
#else
#define HAVE_HW_WATCHPOINTS 0
#endif

/* ===== ELF helper macros - use 64-bit for both x86_64 and aarch64 ===== */
#if defined(__x86_64__) || defined(__aarch64__)
#define Elf_Ehdr Elf64_Ehdr
#define Elf_Shdr Elf64_Shdr
#define Elf_Sym Elf64_Sym
#else
#define Elf_Ehdr Elf32_Ehdr
#define Elf_Shdr Elf32_Shdr
#define Elf_Sym Elf32_Sym
#endif

/* ===== DWARF constants ===== */
#define DW_LNS_copy             1
#define DW_LNS_advance_pc       2
#define DW_LNS_advance_line     3
#define DW_LNS_set_file         4
#define DW_LNS_set_column       5
#define DW_LNS_negate_stmt      6
#define DW_LNS_set_basic_block  7
#define DW_LNS_const_add_pc     8
#define DW_LNS_fixed_advance_pc 9

/* ===== R42-B: DWARF Backtrace Implementation ===== */

/* DWARF LEB128 decoding */
static uint64_t read_uleb128(const uint8_t **ptr) {
    uint64_t result = 0;
    int shift = 0;
    uint8_t byte;

    do {
        byte = *(*ptr)++;
        result |= (uint64_t)(byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);

    return result;
}

static int64_t read_sleb128(const uint8_t **ptr) {
    int64_t result = 0;
    int shift = 0;
    uint8_t byte;

    do {
        byte = *(*ptr)++;
        result |= (int64_t)(byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);

    /* Sign extend if necessary */
    if (shift < 64 && (byte & 0x40)) {
        result |= -(1LL << shift);
    }

    return result;
}

/* Find ELF section by name */
static Elf_Shdr* find_section(void *elf_base, const char *name) {
    Elf_Ehdr *ehdr = (Elf_Ehdr *)elf_base;
    Elf_Shdr *shdr = (Elf_Shdr *)((uint8_t *)elf_base + ehdr->e_shoff);
    Elf_Shdr *shstrtab = &shdr[ehdr->e_shstrndx];
    const char *strtab = (const char *)((uint8_t *)elf_base + shstrtab->sh_offset);

    for (int i = 0; i < ehdr->e_shnum; i++) {
        const char *sname = strtab + shdr[i].sh_name;
        if (strcmp(sname, name) == 0) {
            return &shdr[i];
        }
    }

    return NULL;
}

/* Add a line entry to the debugger */
static int add_line_entry(dwarf_debugger_t *dbg, uint64_t addr, const char *file, int line, int column) {
    if (dbg->line_count >= dbg->line_capacity) {
        size_t new_cap = dbg->line_capacity ? dbg->line_capacity * 2 : 256;
        dwarf_line_t *new_lines = realloc(dbg->lines, new_cap * sizeof(dwarf_line_t));
        if (!new_lines) return -1;
        dbg->lines = new_lines;
        dbg->line_capacity = new_cap;
    }

    dbg->lines[dbg->line_count].addr = addr;
    dbg->lines[dbg->line_count].file = file ? strdup(file) : NULL;
    dbg->lines[dbg->line_count].line = line;
    dbg->lines[dbg->line_count].column = column;
    dbg->line_count++;

    return 0;
}

/* Parse DWARF .debug_line section */
static int parse_debug_line(dwarf_debugger_t *dbg, const uint8_t *data, size_t size) {
    const uint8_t *ptr = data;
    const uint8_t *end = data + size;

    while (ptr < end) {
        /* Read compilation unit header */
        uint32_t unit_length = *(uint32_t *)ptr;
        ptr += 4;

        if (unit_length == 0xffffffff) {
            /* 64-bit DWARF (skip for now) */
            ptr += 8;
            unit_length = *(uint64_t *)(ptr - 8);
        }

        const uint8_t *unit_end = ptr + unit_length;
        if (unit_end > end) break;

        uint16_t version = *(uint16_t *)ptr; ptr += 2;
        if (version > 4) {
            /* Unsupported version */
            ptr = unit_end;
            continue;
        }

        uint32_t header_length = *(uint32_t *)ptr; ptr += 4;
        const uint8_t *header_end = ptr + header_length;

        uint8_t min_insn_length = *ptr++;
        uint8_t default_is_stmt = *ptr++;
        int8_t line_base = *ptr++;
        uint8_t line_range = *ptr++;
        uint8_t opcode_base = *ptr++;

        /* Skip standard opcode lengths */
        ptr += (opcode_base - 1);

        /* Read directory table */
        const char *dirs[256];
        int dir_count = 0;
        dirs[dir_count++] = ".";  /* Directory 0 is current dir */

        while (*ptr != 0 && ptr < header_end) {
            dirs[dir_count++] = (const char *)ptr;
            ptr += strlen((const char *)ptr) + 1;
        }
        ptr++; /* Skip null terminator */

        /* Read file table */
        const char *files[256];
        int file_count = 0;
        files[file_count++] = NULL;  /* File 0 is not used */

        while (*ptr != 0 && ptr < header_end) {
            files[file_count++] = (const char *)ptr;
            ptr += strlen((const char *)ptr) + 1;
            read_uleb128(&ptr);  /* dir_index */
            read_uleb128(&ptr);  /* mtime */
            read_uleb128(&ptr);  /* size */
        }
        ptr++; /* Skip null terminator */

        /* State machine registers */
        uint64_t address = 0;
        int file_idx = 1;
        int line = 1;
        int column = 0;
        int is_stmt = default_is_stmt;

        /* Process line number program */
        while (ptr < unit_end) {
            uint8_t opcode = *ptr++;

            if (opcode == 0) {
                /* Extended opcode */
                uint64_t ext_len = read_uleb128(&ptr);
                uint8_t ext_op = *ptr++;

                if (ext_op == 1) {
                    /* DW_LNE_end_sequence */
                    address = 0;
                    file_idx = 1;
                    line = 1;
                    column = 0;
                } else if (ext_op == 2) {
                    /* DW_LNE_set_address */
                    address = *(uint64_t *)ptr;
                    ptr += 8;
                } else {
                    ptr += ext_len - 1;
                }
            } else if (opcode < opcode_base) {
                /* Standard opcode */
                switch (opcode) {
                    case DW_LNS_copy:
                        if (file_idx > 0 && file_idx < file_count) {
                            add_line_entry(dbg, address, files[file_idx], line, column);
                        }
                        break;
                    case DW_LNS_advance_pc:
                        address += read_uleb128(&ptr) * min_insn_length;
                        break;
                    case DW_LNS_advance_line:
                        line += read_sleb128(&ptr);
                        break;
                    case DW_LNS_set_file:
                        file_idx = read_uleb128(&ptr);
                        break;
                    case DW_LNS_set_column:
                        column = read_uleb128(&ptr);
                        break;
                    case DW_LNS_const_add_pc:
                        address += ((255 - opcode_base) / line_range) * min_insn_length;
                        break;
                    case DW_LNS_fixed_advance_pc:
                        address += *(uint16_t *)ptr;
                        ptr += 2;
                        break;
                    default:
                        /* Skip unknown opcode */
                        break;
                }
            } else {
                /* Special opcode */
                uint8_t adjusted = opcode - opcode_base;
                address += (adjusted / line_range) * min_insn_length;
                line += line_base + (adjusted % line_range);

                if (file_idx > 0 && file_idx < file_count) {
                    add_line_entry(dbg, address, files[file_idx], line, column);
                }
            }
        }
    }

    return 0;
}

/* Initialize DWARF debugger */
int dwarf_debugger_init(dwarf_debugger_t *dbg) {
    memset(dbg, 0, sizeof(dwarf_debugger_t));
    return 0;
}

/* Free DWARF debugger resources */
void dwarf_debugger_free(dwarf_debugger_t *dbg) {
    if (dbg->lines) {
        for (size_t i = 0; i < dbg->line_count; i++) {
            free((void *)dbg->lines[i].file);
        }
        free(dbg->lines);
    }

    if (dbg->funcs) {
        for (size_t i = 0; i < dbg->func_count; i++) {
            free((void *)dbg->funcs[i].name);
        }
        free(dbg->funcs);
    }

    if (dbg->elf_base && dbg->elf_size > 0) {
        munmap(dbg->elf_base, dbg->elf_size);
    }

    memset(dbg, 0, sizeof(dwarf_debugger_t));
}

/* Load DWARF debug information from executable */
int dwarf_debugger_load_dwarf(dwarf_debugger_t *dbg, const char *exe_path) {
    int fd = open(exe_path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s\n", exe_path);
        return -1;
    }

    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return -1;
    }

    void *file_base = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (file_base == MAP_FAILED) {
        fprintf(stderr, "Failed to mmap %s\n", exe_path);
        return -1;
    }

    dbg->elf_base = file_base;
    dbg->elf_size = st.st_size;
    strncpy(dbg->exe_path, exe_path, sizeof(dbg->exe_path) - 1);

    void *elf_base = file_base;

    /* Check if this is an APE (Actually Portable Executable) */
    /* APE format starts with MZ (DOS) header, ELF is embedded inside */
    if (st.st_size > 2 && ((uint8_t *)file_base)[0] == 0x4d && ((uint8_t *)file_base)[1] == 0x5a) {
        /* This is an APE file - for now, skip DWARF parsing */
        /* Full implementation would extract ELF from APE or use separate debug symbols */
        fprintf(stderr, "Note: APE format detected - DWARF parsing from APE not yet implemented\n");
        fprintf(stderr, "      Use native ELF binary or separate debug symbols for source-level debugging\n");
        return 0;  /* Not an error, just no debug info available */
    }

    /* Check ELF header */
    Elf_Ehdr *ehdr = (Elf_Ehdr *)elf_base;
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        fprintf(stderr, "Not a valid ELF file (use native ELF or APE with embedded ELF)\n");
        return -1;
    }

    /* Find and parse .debug_line section */
    Elf_Shdr *debug_line = find_section(elf_base, ".debug_line");
    if (debug_line) {
        const uint8_t *data = (const uint8_t *)elf_base + debug_line->sh_offset;
        parse_debug_line(dbg, data, debug_line->sh_size);
    } else {
        fprintf(stderr, "Warning: No .debug_line section found (compile with -g)\n");
    }

    return 0;
}

/* Binary search for address in line table */
dwarf_line_t* dwarf_debugger_addr_to_line(dwarf_debugger_t *dbg, void *addr) {
    if (!dbg->lines || dbg->line_count == 0) {
        return NULL;
    }

    uint64_t target = (uint64_t)addr;
    dwarf_line_t *best = NULL;

    /* Linear search for closest match (could optimize with binary search) */
    for (size_t i = 0; i < dbg->line_count; i++) {
        if (dbg->lines[i].addr <= target) {
            if (!best || dbg->lines[i].addr > best->addr) {
                best = &dbg->lines[i];
            }
        }
    }

    return best;
}

/* Get function name for address (simplified) */
const char* dwarf_debugger_addr_to_func(dwarf_debugger_t *dbg, void *addr) {
    uint64_t target = (uint64_t)addr;

    for (size_t i = 0; i < dbg->func_count; i++) {
        if (target >= dbg->funcs[i].low_pc && target < dbg->funcs[i].high_pc) {
            return dbg->funcs[i].name;
        }
    }

    return NULL;
}

/* Capture backtrace using frame pointers */
int dwarf_debugger_backtrace(dwarf_debugger_t *dbg, void **frames, int max_frames) {
    int count = 0;
    void **fp;

    /* Get current frame pointer (RBP/FP) */
#ifdef __x86_64__
    __asm__ volatile ("movq %%rbp, %0" : "=r"(fp));
#elif defined(__aarch64__)
    __asm__ volatile ("mov %0, x29" : "=r"(fp));
#else
    return 0;  /* Not implemented for other architectures */
#endif

    /* Walk the stack using frame pointers */
    while (count < max_frames && fp != NULL) {
        /* Return address is at fp[1] */
        void *ret_addr = fp[1];
        if (!ret_addr) break;

        frames[count++] = ret_addr;

        /* Previous frame pointer is at fp[0] */
        void **prev_fp = (void **)fp[0];

        /* Sanity check: frame pointer should grow (stack grows down) */
        if (prev_fp <= fp) break;

        fp = prev_fp;
    }

    return count;
}

/* Fill backtrace with source information */
int dwarf_debugger_fill_backtrace(dwarf_debugger_t *dbg) {
    void *addrs[MAX_BACKTRACE_FRAMES];

    int count = dwarf_debugger_backtrace(dbg, addrs, MAX_BACKTRACE_FRAMES);
    dbg->frame_count = count;
    dbg->current_frame = 0;

    for (int i = 0; i < count; i++) {
        dbg->frames[i].addr = addrs[i];

        dwarf_line_t *line = dwarf_debugger_addr_to_line(dbg, addrs[i]);
        if (line) {
            dbg->frames[i].file = line->file;
            dbg->frames[i].line = line->line;
        } else {
            dbg->frames[i].file = NULL;
            dbg->frames[i].line = 0;
        }

        const char *func = dwarf_debugger_addr_to_func(dbg, addrs[i]);
        dbg->frames[i].func_name = func;
    }

    return count;
}

/* Print formatted backtrace */
void dwarf_debugger_print_backtrace(dwarf_debugger_t *dbg) {
    printf("Backtrace (%d frames):\n", dbg->frame_count);

    for (int i = 0; i < dbg->frame_count; i++) {
        printf("#%-2d 0x%016lx", i, (unsigned long)dbg->frames[i].addr);

        if (dbg->frames[i].func_name) {
            printf(" in %s", dbg->frames[i].func_name);
        }

        if (dbg->frames[i].file && dbg->frames[i].line > 0) {
            printf(" at %s:%d", dbg->frames[i].file, dbg->frames[i].line);
        }

        printf("\n");
    }
}

/* Navigate stack frames */
int dwarf_debugger_frame_up(dwarf_debugger_t *dbg) {
    if (dbg->current_frame < dbg->frame_count - 1) {
        dbg->current_frame++;
        return 0;
    }
    return -1;
}

int dwarf_debugger_frame_down(dwarf_debugger_t *dbg) {
    if (dbg->current_frame > 0) {
        dbg->current_frame--;
        return 0;
    }
    return -1;
}

/* Print current frame info */
void dwarf_debugger_print_frame_info(dwarf_debugger_t *dbg) {
    if (dbg->current_frame >= dbg->frame_count) {
        printf("No current frame\n");
        return;
    }

    stack_frame_t *frame = &dbg->frames[dbg->current_frame];

    printf("Frame #%d: 0x%016lx", dbg->current_frame, (unsigned long)frame->addr);

    if (frame->func_name) {
        printf(" in %s", frame->func_name);
    }

    if (frame->file && frame->line > 0) {
        printf(" at %s:%d", frame->file, frame->line);
    }

    printf("\n");
}

/* List source code around a line */
int dwarf_debugger_list_source(dwarf_debugger_t *dbg, const char *file, int line, int context) {
    FILE *f = fopen(file, "r");
    if (!f) {
        fprintf(stderr, "Cannot open source file: %s\n", file);
        return -1;
    }

    int start = line - context;
    int end = line + context;
    if (start < 1) start = 1;

    char buf[1024];
    int current = 1;

    while (fgets(buf, sizeof(buf), f)) {
        if (current >= start && current <= end) {
            if (current == line) {
                printf("=> %4d  %s", current, buf);
            } else {
                printf("   %4d  %s", current, buf);
            }
        }
        current++;
        if (current > end) break;
    }

    fclose(f);
    return 0;
}

/* ===== R42-C: Hardware Watchpoints Implementation (x86-64) ===== */

#if HAVE_HW_WATCHPOINTS

/* DR7 control register bit positions */
#define DR7_L0 0   // Local enable DR0
#define DR7_L1 2   // Local enable DR1
#define DR7_L2 4   // Local enable DR2
#define DR7_L3 6   // Local enable DR3

#define DR7_RW0 16  // R/W for DR0 (2 bits)
#define DR7_RW1 20  // R/W for DR1
#define DR7_RW2 24  // R/W for DR2
#define DR7_RW3 28  // R/W for DR3

#define DR7_LEN0 18  // Length for DR0 (2 bits)
#define DR7_LEN1 22  // Length for DR1
#define DR7_LEN2 26  // Length for DR2
#define DR7_LEN3 30  // Length for DR3

/* DR6 status register bit positions */
#define DR6_B0 0   // Breakpoint 0 triggered
#define DR6_B1 1   // Breakpoint 1 triggered
#define DR6_B2 2   // Breakpoint 2 triggered
#define DR6_B3 3   // Breakpoint 3 triggered

/* R/W field values (2 bits) */
#define DR7_RW_EXECUTE 0b00    // Break on instruction execution
#define DR7_RW_WRITE   0b01    // Break on data write
#define DR7_RW_IO      0b10    // Break on I/O (not supported)
#define DR7_RW_ACCESS  0b11    // Break on data read or write

/* LEN field values (2 bits) */
#define DR7_LEN_1 0b00  // 1 byte
#define DR7_LEN_2 0b01  // 2 bytes
#define DR7_LEN_8 0b10  // 8 bytes
#define DR7_LEN_4 0b11  // 4 bytes

hw_debugger_t* hw_debugger_init(pid_t child_pid) {
    hw_debugger_t *dbg = calloc(1, sizeof(hw_debugger_t));
    if (!dbg) return NULL;

    dbg->child_pid = child_pid;
    dbg->num_watchpoints = 0;

    return dbg;
}

void hw_debugger_cleanup(hw_debugger_t *dbg) {
    if (!dbg) return;

    /* Clear all watchpoints */
    for (int i = 0; i < MAX_WATCHPOINTS; i++) {
        if (dbg->watchpoints[i].active) {
            hw_debugger_clear_watchpoint(dbg, i);
        }
    }

    free(dbg);
}

static unsigned long encode_len(size_t len) {
    switch (len) {
        case 1: return DR7_LEN_1;
        case 2: return DR7_LEN_2;
        case 4: return DR7_LEN_4;
        case 8: return DR7_LEN_8;
        default:
            fprintf(stderr, "Watchpoint size must be 1, 2, 4, or 8 bytes (got %zu)\n", len);
            return DR7_LEN_1;
    }
}

static unsigned long encode_type(watchpoint_type_t type) {
    switch (type) {
        case WATCHPOINT_WRITE:  return DR7_RW_WRITE;
        case WATCHPOINT_READ:   return DR7_RW_ACCESS;  // Read-only not supported, use access
        case WATCHPOINT_ACCESS: return DR7_RW_ACCESS;
        default: return DR7_RW_WRITE;
    }
}

int hw_debugger_set_watchpoint(hw_debugger_t *dbg, void *addr, size_t len, watchpoint_type_t type) {
    if (!dbg) return -1;

    /* Find free watchpoint slot */
    int wp_id = -1;
    for (int i = 0; i < MAX_WATCHPOINTS; i++) {
        if (!dbg->watchpoints[i].active) {
            wp_id = i;
            break;
        }
    }

    if (wp_id == -1) {
        fprintf(stderr, "No free watchpoint slots (max %d)\n", MAX_WATCHPOINTS);
        return -1;
    }

    /* Set DRx address register */
    unsigned long dr_offset = offsetof(struct user, u_debugreg[wp_id]);
    if (ptrace(PTRACE_POKEUSER, dbg->child_pid, dr_offset, (unsigned long)addr) == -1) {
        fprintf(stderr, "Failed to set DR%d address: %s\n", wp_id, strerror(errno));
        return -1;
    }

    /* Read current DR7 */
    unsigned long dr7_offset = offsetof(struct user, u_debugreg[7]);
    errno = 0;
    unsigned long dr7 = ptrace(PTRACE_PEEKUSER, dbg->child_pid, dr7_offset, 0);
    if (errno != 0) {
        fprintf(stderr, "Failed to read DR7: %s\n", strerror(errno));
        return -1;
    }

    /* Calculate bit positions for this watchpoint */
    int local_enable_bit = DR7_L0 + (wp_id * 2);
    int rw_bit = DR7_RW0 + (wp_id * 4);
    int len_bit = DR7_LEN0 + (wp_id * 4);

    /* Set local enable bit */
    dr7 |= (1UL << local_enable_bit);

    /* Set R/W field (2 bits) */
    unsigned long rw_value = encode_type(type);
    dr7 &= ~(0b11UL << rw_bit);  // Clear existing bits
    dr7 |= (rw_value << rw_bit);

    /* Set LEN field (2 bits) */
    unsigned long len_value = encode_len(len);
    dr7 &= ~(0b11UL << len_bit);  // Clear existing bits
    dr7 |= (len_value << len_bit);

    /* Write DR7 */
    if (ptrace(PTRACE_POKEUSER, dbg->child_pid, dr7_offset, dr7) == -1) {
        fprintf(stderr, "Failed to set DR7: %s\n", strerror(errno));
        return -1;
    }

    /* Update internal state */
    dbg->watchpoints[wp_id].addr = addr;
    dbg->watchpoints[wp_id].len = len;
    dbg->watchpoints[wp_id].type = type;
    dbg->watchpoints[wp_id].active = 1;
    dbg->num_watchpoints++;

    printf("Watchpoint %d set: addr=%p, len=%zu, type=%s\n",
           wp_id, addr, len,
           type == WATCHPOINT_WRITE ? "write" :
           type == WATCHPOINT_READ ? "read" : "access");

    return wp_id;
}

int hw_debugger_clear_watchpoint(hw_debugger_t *dbg, int wp_id) {
    if (!dbg || wp_id < 0 || wp_id >= MAX_WATCHPOINTS) return -1;
    if (!dbg->watchpoints[wp_id].active) return -1;

    /* Clear DRx address register */
    unsigned long dr_offset = offsetof(struct user, u_debugreg[wp_id]);
    if (ptrace(PTRACE_POKEUSER, dbg->child_pid, dr_offset, 0) == -1) {
        fprintf(stderr, "Failed to clear DR%d: %s\n", wp_id, strerror(errno));
        return -1;
    }

    /* Read DR7 */
    unsigned long dr7_offset = offsetof(struct user, u_debugreg[7]);
    errno = 0;
    unsigned long dr7 = ptrace(PTRACE_PEEKUSER, dbg->child_pid, dr7_offset, 0);
    if (errno != 0) {
        fprintf(stderr, "Failed to read DR7: %s\n", strerror(errno));
        return -1;
    }

    /* Clear local enable bit */
    int local_enable_bit = DR7_L0 + (wp_id * 2);
    dr7 &= ~(1UL << local_enable_bit);

    /* Write DR7 */
    if (ptrace(PTRACE_POKEUSER, dbg->child_pid, dr7_offset, dr7) == -1) {
        fprintf(stderr, "Failed to update DR7: %s\n", strerror(errno));
        return -1;
    }

    /* Update internal state */
    dbg->watchpoints[wp_id].active = 0;
    dbg->num_watchpoints--;

    printf("Watchpoint %d cleared\n", wp_id);
    return 0;
}

int hw_debugger_get_watchpoint_hit(hw_debugger_t *dbg) {
    if (!dbg) return -1;

    /* Read DR6 status register */
    unsigned long dr6_offset = offsetof(struct user, u_debugreg[6]);
    errno = 0;
    unsigned long dr6 = ptrace(PTRACE_PEEKUSER, dbg->child_pid, dr6_offset, 0);
    if (errno != 0) {
        fprintf(stderr, "Failed to read DR6: %s\n", strerror(errno));
        return -1;
    }

    /* Check which watchpoint triggered */
    for (int i = 0; i < MAX_WATCHPOINTS; i++) {
        if (dbg->watchpoints[i].active && (dr6 & (1UL << i))) {
            /* Clear the status bit */
            dr6 &= ~(1UL << i);
            ptrace(PTRACE_POKEUSER, dbg->child_pid, dr6_offset, dr6);
            return i;
        }
    }

    return -1;  // No watchpoint hit
}

void hw_debugger_list_watchpoints(hw_debugger_t *dbg) {
    if (!dbg) return;

    printf("Watchpoints:\n");
    int found = 0;
    for (int i = 0; i < MAX_WATCHPOINTS; i++) {
        if (dbg->watchpoints[i].active) {
            watchpoint_t *wp = &dbg->watchpoints[i];
            printf("  %d: addr=%p, len=%zu, type=%s\n",
                   i, wp->addr, wp->len,
                   wp->type == WATCHPOINT_WRITE ? "write" :
                   wp->type == WATCHPOINT_READ ? "read" : "access");
            found = 1;
        }
    }
    if (!found) {
        printf("  (none)\n");
    }
}

#else /* !HAVE_HW_WATCHPOINTS */

/* Stub implementations for platforms without hardware watchpoint support */
hw_debugger_t* hw_debugger_init(pid_t child_pid) {
    fprintf(stderr, "Hardware watchpoints not supported on this architecture\n");
    return NULL;
}

void hw_debugger_cleanup(hw_debugger_t *dbg) {
    (void)dbg;
}

int hw_debugger_set_watchpoint(hw_debugger_t *dbg, void *addr, size_t len, watchpoint_type_t type) {
    (void)dbg; (void)addr; (void)len; (void)type;
    fprintf(stderr, "Hardware watchpoints not supported on this architecture\n");
    return -1;
}

int hw_debugger_clear_watchpoint(hw_debugger_t *dbg, int wp_id) {
    (void)dbg; (void)wp_id;
    return -1;
}

int hw_debugger_get_watchpoint_hit(hw_debugger_t *dbg) {
    (void)dbg;
    return -1;
}

void hw_debugger_list_watchpoints(hw_debugger_t *dbg) {
    (void)dbg;
    printf("Hardware watchpoints not supported on this architecture\n");
}

#endif /* HAVE_HW_WATCHPOINTS */
