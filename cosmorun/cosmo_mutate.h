/**
 * @file cosmo_mutate.h
 * @brief Mutation Testing Framework
 *
 * Implements mutation testing to verify test suite quality by injecting
 * bugs into source code and checking if tests catch them.
 */

#ifndef COSMO_MUTATE_H
#define COSMO_MUTATE_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Mutation operator types */
typedef enum {
    MUT_FLIP_OPERATOR,     /* Flip operators: + → -, == → !=, && → || */
    MUT_CHANGE_CONSTANT,   /* Change constants: 0 → 1, 1 → 0, N → N+1 */
    MUT_DELETE_STATEMENT,  /* Delete single statement (experimental) */
    MUT_NEGATE_CONDITION,  /* Negate conditions: if (x) → if (!x) */
    MUT_REPLACE_RETURN,    /* Replace returns: return x → return 0 */
    MUT_ALL                /* All mutation types */
} mutation_op_t;

/* Mutation result status */
typedef enum {
    MUTANT_PENDING,    /* Not yet tested */
    MUTANT_KILLED,     /* Test detected the mutation (good) */
    MUTANT_SURVIVED,   /* Test passed with mutation (bad) */
    MUTANT_ERROR       /* Compilation or runtime error */
} mutant_status_t;

/* Individual mutant information */
typedef struct {
    const char *file;          /* Source file path */
    int line;                  /* Line number of mutation */
    int column;                /* Column number */
    mutation_op_t op;          /* Mutation operator used */
    char original[128];        /* Original code */
    char mutated[128];         /* Mutated code */
    mutant_status_t status;    /* Test result status */
    char error_msg[256];       /* Error message if any */
} mutant_t;

/* Mutator context (opaque) */
typedef struct mutator_t mutator_t;

/**
 * Create a new mutator for a source file
 * @param source_file Path to C source file
 * @return Mutator context or NULL on error
 */
mutator_t* mutator_create(const char *source_file);

/**
 * Generate mutants from source code
 * @param mut Mutator context
 * @param ops Mutation operator flags (bitwise OR of mutation_op_t)
 * @param max_mutants Maximum number of mutants to generate (0 = unlimited)
 * @return Number of mutants generated, or -1 on error
 */
int mutator_generate_mutants(mutator_t *mut, int ops, int max_mutants);

/**
 * Apply a specific mutant to source code
 * @param mut Mutator context
 * @param mutant_id Mutant index (0-based)
 * @param output_file Path to write mutated source
 * @return 0 on success, -1 on error
 */
int mutator_apply_mutant(mutator_t *mut, int mutant_id, const char *output_file);

/**
 * Test a mutant by compiling and running test command
 * @param mut Mutator context
 * @param mutant_id Mutant index
 * @param test_cmd Test command to run (NULL = just compile)
 * @return 0 if mutant killed, 1 if survived, -1 on error
 */
int mutator_test_mutant(mutator_t *mut, int mutant_id, const char *test_cmd);

/**
 * Get mutant information
 * @param mut Mutator context
 * @param mutant_id Mutant index
 * @return Pointer to mutant info or NULL on error
 */
const mutant_t* mutator_get_mutant(mutator_t *mut, int mutant_id);

/**
 * Get total number of mutants
 * @param mut Mutator context
 * @return Number of mutants
 */
int mutator_get_count(mutator_t *mut);

/**
 * Calculate mutation score (killed / total * 100%)
 * @param mut Mutator context
 * @return Mutation score (0.0 - 100.0)
 */
double mutator_get_score(mutator_t *mut);

/**
 * Print mutation testing report
 * @param mut Mutator context
 * @param fp Output file stream
 */
void mutator_print_report(mutator_t *mut, FILE *fp);

/**
 * Destroy mutator and free resources
 * @param mut Mutator context
 */
void mutator_destroy(mutator_t *mut);

#ifdef __cplusplus
}
#endif

#endif /* COSMO_MUTATE_H */
