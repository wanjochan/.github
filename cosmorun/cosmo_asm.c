/* cosmo_asm.c - Inline Assembly Support
 * Provides parsing and code generation for GCC-style inline assembly
 */

#include "cosmo_asm.h"
#include "cosmo_libc.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

// ============================================================================
// Architecture Detection
// ============================================================================

asm_arch_t cosmo_asm_get_arch(void) {
#if defined(__x86_64__) || defined(_M_X64)
    return ASM_ARCH_X86_64;
#elif defined(__aarch64__) || defined(__arm64__) || defined(_M_ARM64)
    return ASM_ARCH_ARM64;
#elif defined(__i386__) || defined(_M_IX86)
    return ASM_ARCH_X86;
#elif defined(__arm__) || defined(_M_ARM)
    return ASM_ARCH_ARM32;
#else
    return ASM_ARCH_UNKNOWN;
#endif
}

// ============================================================================
// Block Initialization
// ============================================================================

void cosmo_asm_init_block(asm_block_t *block) {
    if (!block) return;

    memset(block, 0, sizeof(asm_block_t));
    block->dialect = ASM_DIALECT_ATT;  // Default to AT&T syntax
    block->is_volatile = false;
    block->is_goto = false;
}

void cosmo_asm_free_block(asm_block_t *block) {
    // Nothing to free currently (static allocation)
    (void)block;
}

// ============================================================================
// String Parsing Helpers
// ============================================================================

static char *skip_whitespace(char *p) {
    while (*p && isspace(*p)) p++;
    return p;
}

static char *parse_string_literal(char *p, char *dest, size_t max_len) {
    p = skip_whitespace(p);
    if (*p != '"') return NULL;
    p++; // skip opening quote

    size_t i = 0;
    while (*p && *p != '"' && i < max_len - 1) {
        if (*p == '\\' && *(p+1)) {
            p++; // skip escape
            switch (*p) {
                case 'n': dest[i++] = '\n'; break;
                case 't': dest[i++] = '\t'; break;
                case 'r': dest[i++] = '\r'; break;
                case '\\': dest[i++] = '\\'; break;
                case '"': dest[i++] = '"'; break;
                default: dest[i++] = *p; break;
            }
            p++;
        } else {
            dest[i++] = *p++;
        }
    }
    dest[i] = '\0';

    if (*p != '"') return NULL;
    return p + 1;
}

// ============================================================================
// Constraint Parsing
// ============================================================================

int cosmo_asm_parse_constraint(const char *constraint, asm_operand_t *operand) {
    if (!constraint || !operand) return -1;

    strncpy(operand->constraint, constraint, sizeof(operand->constraint) - 1);
    operand->constraint[sizeof(operand->constraint) - 1] = '\0';

    const char *p = constraint;

    // Parse output/input markers
    if (*p == '=') {
        operand->is_output = 1;
        operand->is_input = 0;
        operand->type = ASM_CONSTRAINT_OUTPUT;
        p++;
    } else if (*p == '+') {
        operand->is_output = 1;
        operand->is_input = 1;
        operand->type = ASM_CONSTRAINT_READWRITE;
        p++;
    } else {
        operand->is_output = 0;
        operand->is_input = 1;
        operand->type = ASM_CONSTRAINT_INPUT;
    }

    // Parse constraint type
    if (*p == 'r') {
        operand->type = (operand->type == ASM_CONSTRAINT_OUTPUT) ?
                        ASM_CONSTRAINT_OUTPUT : ASM_CONSTRAINT_REGISTER;
    } else if (*p == 'm') {
        operand->type = ASM_CONSTRAINT_MEMORY;
    } else if (*p == 'i') {
        operand->type = ASM_CONSTRAINT_IMMEDIATE;
    }

    return 0;
}

// ============================================================================
// Simple Assembly Parsing (no operands)
// ============================================================================

int cosmo_asm_parse_simple(const char *asm_str, asm_block_t *block) {
    if (!asm_str || !block) return -1;

    cosmo_asm_init_block(block);
    strncpy(block->asm_string, asm_str, sizeof(block->asm_string) - 1);
    block->asm_string[sizeof(block->asm_string) - 1] = '\0';

    return 0;
}

// ============================================================================
// Extended Assembly Parsing (with operands)
// ============================================================================

int cosmo_asm_parse_extended(const char *input, asm_block_t *block) {
    if (!input || !block) return -1;

    cosmo_asm_init_block(block);

    // Parse format: "asm_string" : outputs : inputs : clobbers
    char *p = (char *)input;

    // Parse assembly string
    p = parse_string_literal(p, block->asm_string, sizeof(block->asm_string));
    if (!p) return -1;

    p = skip_whitespace(p);
    if (*p != ':') {
        // Simple asm with no operands
        return 0;
    }
    p++; // skip ':'

    // Parse output operands
    p = skip_whitespace(p);
    while (*p && *p != ':' && *p != ')') {
        if (*p == ',') {
            p++;
            p = skip_whitespace(p);
            continue;
        }

        if (block->num_outputs >= MAX_ASM_OPERANDS) return -1;

        // Parse constraint
        char constraint[32];
        p = parse_string_literal(p, constraint, sizeof(constraint));
        if (!p) return -1;

        asm_operand_t *op = &block->outputs[block->num_outputs];
        cosmo_asm_parse_constraint(constraint, op);

        // Parse variable name
        p = skip_whitespace(p);
        if (*p == '(') {
            p++;
            p = skip_whitespace(p);
            int i = 0;
            while (*p && *p != ')' && i < 63) {
                op->var_name[i++] = *p++;
            }
            op->var_name[i] = '\0';
            if (*p == ')') p++;
        }

        block->num_outputs++;
        p = skip_whitespace(p);
    }

    if (*p == ':') p++; // skip to inputs

    // Parse input operands
    p = skip_whitespace(p);
    while (*p && *p != ':' && *p != ')') {
        if (*p == ',') {
            p++;
            p = skip_whitespace(p);
            continue;
        }

        if (block->num_inputs >= MAX_ASM_OPERANDS) return -1;

        // Parse constraint
        char constraint[32];
        p = parse_string_literal(p, constraint, sizeof(constraint));
        if (!p) return -1;

        asm_operand_t *op = &block->inputs[block->num_inputs];
        cosmo_asm_parse_constraint(constraint, op);

        // Parse variable name
        p = skip_whitespace(p);
        if (*p == '(') {
            p++;
            p = skip_whitespace(p);
            int i = 0;
            while (*p && *p != ')' && i < 63) {
                op->var_name[i++] = *p++;
            }
            op->var_name[i] = '\0';
            if (*p == ')') p++;
        }

        block->num_inputs++;
        p = skip_whitespace(p);
    }

    if (*p == ':') p++; // skip to clobbers

    // Parse clobbers
    p = skip_whitespace(p);
    while (*p && *p != ')') {
        if (*p == ',') {
            p++;
            p = skip_whitespace(p);
            continue;
        }

        if (block->num_clobbers >= MAX_ASM_CLOBBERS) return -1;

        char clobber[32];
        p = parse_string_literal(p, clobber, sizeof(clobber));
        if (!p) return -1;

        strncpy(block->clobbers[block->num_clobbers], clobber, 31);
        block->clobbers[block->num_clobbers][31] = '\0';
        block->num_clobbers++;

        p = skip_whitespace(p);
    }

    return 0;
}

int cosmo_asm_parse(const char *input, asm_block_t *block) {
    if (!input || !block) return -1;

    // Check for extended vs simple asm
    const char *p = strchr(input, ':');
    if (p) {
        return cosmo_asm_parse_extended(input, block);
    } else {
        // Extract string literal
        char asm_str[MAX_ASM_STRING];
        if (!parse_string_literal((char *)input, asm_str, sizeof(asm_str))) {
            return -1;
        }
        return cosmo_asm_parse_simple(asm_str, block);
    }
}

// ============================================================================
// Validation
// ============================================================================

bool cosmo_asm_validate_register(const char *reg, asm_arch_t arch) {
    if (!reg) return false;

    if (arch == ASM_ARCH_X86_64) {
        // Common x86-64 registers
        const char *regs[] = {
            "rax", "rbx", "rcx", "rdx", "rsi", "rdi", "rbp", "rsp",
            "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15",
            "eax", "ebx", "ecx", "edx", "esi", "edi", "ebp", "esp",
            "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
            NULL
        };
        for (int i = 0; regs[i]; i++) {
            if (strcmp(reg, regs[i]) == 0) return true;
        }
    } else if (arch == ASM_ARCH_ARM64) {
        // ARM64 registers
        if (reg[0] == 'x' || reg[0] == 'w' || reg[0] == 'v' || reg[0] == 'q') {
            return true;
        }
    }

    return false;
}

bool cosmo_asm_validate_instruction(const char *instr, asm_arch_t arch) {
    if (!instr) return false;
    // Basic validation - just check non-empty
    return strlen(instr) > 0;
}

bool cosmo_asm_is_privileged(const char *asm_str) {
    if (!asm_str) return false;

    // Check for privileged x86 instructions
    const char *priv[] = {
        "hlt", "cli", "sti", "lgdt", "lidt", "lldt", "ltr",
        "mov cr", "mov dr", "in ", "out ", "rdmsr", "wrmsr",
        NULL
    };

    for (int i = 0; priv[i]; i++) {
        if (strstr(asm_str, priv[i])) return true;
    }

    return false;
}

// ============================================================================
// Debug Output
// ============================================================================

void cosmo_asm_print_block(asm_block_t *block) {
    if (!block) return;

    printf("=== ASM Block ===\n");
    printf("Assembly: %s\n", block->asm_string);
    printf("Outputs (%d):\n", block->num_outputs);
    for (int i = 0; i < block->num_outputs; i++) {
        printf("  [%d] %s (%s)\n", i, block->outputs[i].constraint,
               block->outputs[i].var_name);
    }
    printf("Inputs (%d):\n", block->num_inputs);
    for (int i = 0; i < block->num_inputs; i++) {
        printf("  [%d] %s (%s)\n", i, block->inputs[i].constraint,
               block->inputs[i].var_name);
    }
    printf("Clobbers (%d):\n", block->num_clobbers);
    for (int i = 0; i < block->num_clobbers; i++) {
        printf("  [%d] %s\n", i, block->clobbers[i]);
    }
    printf("=================\n");
}

// ============================================================================
// Common Inline Assembly Helpers
// ============================================================================

#if defined(__x86_64__) || defined(_M_X64)

void cosmo_asm_cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx,
                     uint32_t *ecx, uint32_t *edx) {
    __asm__ __volatile__(
        "cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf)
    );
}

uint64_t cosmo_asm_rdtsc(void) {
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

void cosmo_asm_mfence(void) {
    __asm__ __volatile__("mfence" ::: "memory");
}

void cosmo_asm_barrier(void) {
    __asm__ __volatile__("" ::: "memory");
}

#elif defined(__aarch64__) || defined(__arm64__)

void cosmo_asm_cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx,
                     uint32_t *ecx, uint32_t *edx) {
    // CPUID is x86-specific, set to zero on ARM
    *eax = *ebx = *ecx = *edx = 0;
}

uint64_t cosmo_asm_rdtsc(void) {
    uint64_t val;
    __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(val));
    return val;
}

void cosmo_asm_mfence(void) {
    __asm__ __volatile__("dmb sy" ::: "memory");
}

void cosmo_asm_barrier(void) {
    __asm__ __volatile__("" ::: "memory");
}

#else

// Fallback implementations
void cosmo_asm_cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx,
                     uint32_t *ecx, uint32_t *edx) {
    *eax = *ebx = *ecx = *edx = 0;
}

uint64_t cosmo_asm_rdtsc(void) {
    return 0;
}

void cosmo_asm_mfence(void) {
}

void cosmo_asm_barrier(void) {
}

#endif

// ============================================================================
// Code Generation (Stub - for future implementation)
// ============================================================================

void *cosmo_asm_emit(asm_block_t *block, asm_arch_t arch) {
    // For now, return NULL (not implemented)
    // Real implementation would generate machine code
    (void)block;
    (void)arch;
    return NULL;
}

void *cosmo_asm_emit_x86_64(asm_block_t *block) {
    return cosmo_asm_emit(block, ASM_ARCH_X86_64);
}

void *cosmo_asm_emit_arm64(asm_block_t *block) {
    return cosmo_asm_emit(block, ASM_ARCH_ARM64);
}

char *cosmo_asm_generate_gas_syntax(asm_block_t *block) {
    if (!block) return NULL;

    // Simple implementation: return the asm string as-is
    char *result = malloc(strlen(block->asm_string) + 1);
    if (result) {
        strcpy(result, block->asm_string);
    }
    return result;
}

// ============================================================================
// Register Mapping
// ============================================================================

int cosmo_asm_register_to_num(const char *reg, asm_arch_t arch) {
    if (!reg) return -1;

    if (arch == ASM_ARCH_X86_64) {
        if (strcmp(reg, "rax") == 0) return 0;
        if (strcmp(reg, "rcx") == 0) return 1;
        if (strcmp(reg, "rdx") == 0) return 2;
        if (strcmp(reg, "rbx") == 0) return 3;
        if (strcmp(reg, "rsp") == 0) return 4;
        if (strcmp(reg, "rbp") == 0) return 5;
        if (strcmp(reg, "rsi") == 0) return 6;
        if (strcmp(reg, "rdi") == 0) return 7;
    }

    return -1;
}

const char *cosmo_asm_num_to_register(int num, asm_arch_t arch) {
    if (arch == ASM_ARCH_X86_64) {
        const char *regs[] = {
            "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi"
        };
        if (num >= 0 && num < 8) return regs[num];
    }

    return NULL;
}
