/* Unified debugger with DWARF backtrace (R42-B) and hardware watchpoints (R42-C) */
#ifndef COSMO_DEBUGGER_H
#define COSMO_DEBUGGER_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

/* ===== R42-B: DWARF Backtrace Support ===== */

/* Maximum frames in backtrace */
#define MAX_BACKTRACE_FRAMES 128

/* DWARF line entry mapping address to source location */
typedef struct dwarf_line_t {
    uint64_t addr;
    const char *file;
    int line;
    int column;
} dwarf_line_t;

/* DWARF function/subprogram entry */
typedef struct dwarf_func_t {
    uint64_t low_pc;
    uint64_t high_pc;
    const char *name;
} dwarf_func_t;

/* Stack frame for backtrace */
typedef struct stack_frame_t {
    void *addr;              /* Return address */
    const char *func_name;   /* Function name if available */
    const char *file;        /* Source file if available */
    int line;                /* Line number if available */
} stack_frame_t;

/* DWARF-based debugger state */
typedef struct dwarf_debugger_t {
    /* DWARF data */
    dwarf_line_t *lines;
    size_t line_count;
    size_t line_capacity;

    dwarf_func_t *funcs;
    size_t func_count;
    size_t func_capacity;

    /* ELF file mapping */
    void *elf_base;
    size_t elf_size;

    /* Loaded executable path */
    char exe_path[1024];

    /* Current frame index for navigation */
    int current_frame;

    /* Backtrace cache */
    stack_frame_t frames[MAX_BACKTRACE_FRAMES];
    int frame_count;
} dwarf_debugger_t;

/* DWARF debugger API */
int dwarf_debugger_init(dwarf_debugger_t *dbg);
void dwarf_debugger_free(dwarf_debugger_t *dbg);
int dwarf_debugger_load_dwarf(dwarf_debugger_t *dbg, const char *exe_path);
dwarf_line_t* dwarf_debugger_addr_to_line(dwarf_debugger_t *dbg, void *addr);
const char* dwarf_debugger_addr_to_func(dwarf_debugger_t *dbg, void *addr);
int dwarf_debugger_backtrace(dwarf_debugger_t *dbg, void **frames, int max_frames);
int dwarf_debugger_fill_backtrace(dwarf_debugger_t *dbg);
void dwarf_debugger_print_backtrace(dwarf_debugger_t *dbg);
int dwarf_debugger_frame_up(dwarf_debugger_t *dbg);
int dwarf_debugger_frame_down(dwarf_debugger_t *dbg);
void dwarf_debugger_print_frame_info(dwarf_debugger_t *dbg);
int dwarf_debugger_list_source(dwarf_debugger_t *dbg, const char *file, int line, int context);

/* ===== R42-C: Hardware Watchpoints Support (x86-64) ===== */

#define MAX_WATCHPOINTS 4  // DR0-DR3 hardware limitation

typedef enum {
    WATCHPOINT_WRITE,   // Break on write (LEN=0b01)
    WATCHPOINT_READ,    // Break on read (LEN=0b11, rarely supported)
    WATCHPOINT_ACCESS   // Break on read or write (LEN=0b11)
} watchpoint_type_t;

typedef struct {
    void *addr;
    size_t len;
    watchpoint_type_t type;
    int active;
} watchpoint_t;

typedef struct {
    pid_t child_pid;
    watchpoint_t watchpoints[MAX_WATCHPOINTS];
    int num_watchpoints;
} hw_debugger_t;

/* Hardware watchpoint API */
hw_debugger_t* hw_debugger_init(pid_t child_pid);
void hw_debugger_cleanup(hw_debugger_t *dbg);
int hw_debugger_set_watchpoint(hw_debugger_t *dbg, void *addr, size_t len, watchpoint_type_t type);
int hw_debugger_clear_watchpoint(hw_debugger_t *dbg, int wp_id);
int hw_debugger_get_watchpoint_hit(hw_debugger_t *dbg);
void hw_debugger_list_watchpoints(hw_debugger_t *dbg);

/* ===== Legacy API for backward compatibility ===== */
/* Maps to dwarf_debugger_t for now */
typedef dwarf_debugger_t debugger_t;
#define debugger_init dwarf_debugger_init
#define debugger_free dwarf_debugger_free
#define debugger_load_dwarf dwarf_debugger_load_dwarf
#define debugger_addr_to_line dwarf_debugger_addr_to_line
#define debugger_addr_to_func dwarf_debugger_addr_to_func
#define debugger_backtrace dwarf_debugger_backtrace
#define debugger_fill_backtrace dwarf_debugger_fill_backtrace
#define debugger_print_backtrace dwarf_debugger_print_backtrace
#define debugger_frame_up dwarf_debugger_frame_up
#define debugger_frame_down dwarf_debugger_frame_down
#define debugger_print_frame_info dwarf_debugger_print_frame_info
#define debugger_list_source dwarf_debugger_list_source

#endif /* COSMO_DEBUGGER_H */
