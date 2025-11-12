#ifndef COSMO_ASM_H
#define COSMO_ASM_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Inline Assembly Support
// ============================================================================

// Maximum assembly string length
#define MAX_ASM_STRING 4096
#define MAX_ASM_OPERANDS 16
#define MAX_ASM_CLOBBERS 16

// Assembly dialect types
typedef enum {
    ASM_DIALECT_ATT,    // AT&T syntax (default for GCC/Clang on Unix)
    ASM_DIALECT_INTEL   // Intel syntax
} asm_dialect_t;

// Architecture types
typedef enum {
    ASM_ARCH_X86_64,
    ASM_ARCH_ARM64,
    ASM_ARCH_X86,
    ASM_ARCH_ARM32,
    ASM_ARCH_UNKNOWN
} asm_arch_t;

// Operand constraint types
typedef enum {
    ASM_CONSTRAINT_REGISTER,    // "r" - general register
    ASM_CONSTRAINT_MEMORY,      // "m" - memory operand
    ASM_CONSTRAINT_IMMEDIATE,   // "i" - immediate integer
    ASM_CONSTRAINT_OUTPUT,      // "=" - output operand
    ASM_CONSTRAINT_INPUT,       // no prefix - input operand
    ASM_CONSTRAINT_READWRITE,   // "+" - read-write operand
    ASM_CONSTRAINT_EARLY_CLOBBER, // "&" - early clobber
    ASM_CONSTRAINT_SPECIFIC_REG // specific register (rax, rbx, etc.)
} asm_constraint_type_t;

// Operand structure
typedef struct {
    char constraint[32];        // Constraint string (e.g., "=r", "m", "+r")
    char var_name[64];          // Variable name or register
    asm_constraint_type_t type;
    int is_output;
    int is_input;
    int reg_num;                // Register number if specific
} asm_operand_t;

// Inline assembly block structure
typedef struct {
    char asm_string[MAX_ASM_STRING];   // Assembly code string
    asm_operand_t outputs[MAX_ASM_OPERANDS];
    int num_outputs;
    asm_operand_t inputs[MAX_ASM_OPERANDS];
    int num_inputs;
    char clobbers[MAX_ASM_CLOBBERS][32];  // Clobbered registers
    int num_clobbers;
    bool is_volatile;           // volatile asm
    bool is_goto;               // asm goto (C11)
    asm_dialect_t dialect;
} asm_block_t;

// ============================================================================
// Parsing Functions
// ============================================================================

// Parse inline assembly block
// Returns 0 on success, -1 on error
int cosmo_asm_parse(const char *input, asm_block_t *block);

// Parse simple asm (no operands)
// Example: __asm__("nop");
int cosmo_asm_parse_simple(const char *asm_str, asm_block_t *block);

// Parse extended asm (with operands)
// Example: __asm__("addq %1, %0" : "=r"(out) : "r"(in));
int cosmo_asm_parse_extended(const char *input, asm_block_t *block);

// Parse constraint string
int cosmo_asm_parse_constraint(const char *constraint, asm_operand_t *operand);

// ============================================================================
// Validation Functions
// ============================================================================

// Validate assembly instruction syntax
bool cosmo_asm_validate_instruction(const char *instr, asm_arch_t arch);

// Validate register name
bool cosmo_asm_validate_register(const char *reg, asm_arch_t arch);

// Check if assembly uses privileged instructions
bool cosmo_asm_is_privileged(const char *asm_str);

// ============================================================================
// Code Generation Functions
// ============================================================================

// Emit inline assembly code
// Returns pointer to generated code or NULL on error
void *cosmo_asm_emit(asm_block_t *block, asm_arch_t arch);

// Emit machine code for x86-64
void *cosmo_asm_emit_x86_64(asm_block_t *block);

// Emit machine code for ARM64
void *cosmo_asm_emit_arm64(asm_block_t *block);

// Generate assembly code to pass to system assembler
char *cosmo_asm_generate_gas_syntax(asm_block_t *block);

// ============================================================================
// Utility Functions
// ============================================================================

// Get current architecture
asm_arch_t cosmo_asm_get_arch(void);

// Convert register name to number
int cosmo_asm_register_to_num(const char *reg, asm_arch_t arch);

// Get register name from number
const char *cosmo_asm_num_to_register(int num, asm_arch_t arch);

// Initialize asm block
void cosmo_asm_init_block(asm_block_t *block);

// Free resources
void cosmo_asm_free_block(asm_block_t *block);

// Print asm block (for debugging)
void cosmo_asm_print_block(asm_block_t *block);

// ============================================================================
// Common Inline Assembly Patterns
// ============================================================================

// Execute CPUID instruction (x86-64 only)
void cosmo_asm_cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx,
                     uint32_t *ecx, uint32_t *edx);

// Read timestamp counter (x86-64 only)
uint64_t cosmo_asm_rdtsc(void);

// Memory barrier
void cosmo_asm_mfence(void);

// Compiler barrier
void cosmo_asm_barrier(void);

#ifdef __cplusplus
}
#endif

#endif // COSMO_ASM_H
