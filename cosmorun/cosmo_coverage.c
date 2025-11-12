/* cosmo_coverage.c - Coverage tracking implementation
 * Tracks statement and branch execution for code coverage analysis
 */

#define _POSIX_C_SOURCE 200809L
#include "cosmo_coverage.h"
#include <stdlib.h>
#include <string.h>

// Hash table size for coverage tracking
#define COVERAGE_HASH_SIZE 1024
#define COVERAGE_MAX_ITEMS 4096

// Hash table entry for statements
typedef struct stmt_entry_t {
    char *file;
    int line;
    uint32_t count;
    struct stmt_entry_t *next;
} stmt_entry_t;

// Hash table entry for branches
typedef struct branch_entry_t {
    char *file;
    int line;
    uint32_t taken_true;
    uint32_t taken_false;
    struct branch_entry_t *next;
} branch_entry_t;

// Coverage tracking structure
struct coverage_t {
    stmt_entry_t *stmt_table[COVERAGE_HASH_SIZE];
    branch_entry_t *branch_table[COVERAGE_HASH_SIZE];
    int stmt_count;
    int branch_count;
};

// Global coverage instance
coverage_t *g_coverage = NULL;

// Simple hash function for file:line
static uint32_t hash_location(const char *file, int line) {
    uint32_t hash = 5381;
    const char *str = file;

    while (*str) {
        hash = ((hash << 5) + hash) + *str;
        str++;
    }

    hash = ((hash << 5) + hash) + line;
    return hash % COVERAGE_HASH_SIZE;
}

// Create coverage tracker
coverage_t *coverage_create(void) {
    coverage_t *cov = (coverage_t *)calloc(1, sizeof(coverage_t));
    if (!cov) return NULL;

    cov->stmt_count = 0;
    cov->branch_count = 0;

    return cov;
}

// Destroy coverage tracker
void coverage_destroy(coverage_t *cov) {
    if (!cov) return;

    // Free statement table
    for (int i = 0; i < COVERAGE_HASH_SIZE; i++) {
        stmt_entry_t *entry = cov->stmt_table[i];
        while (entry) {
            stmt_entry_t *next = entry->next;
            free(entry->file);
            free(entry);
            entry = next;
        }
    }

    // Free branch table
    for (int i = 0; i < COVERAGE_HASH_SIZE; i++) {
        branch_entry_t *entry = cov->branch_table[i];
        while (entry) {
            branch_entry_t *next = entry->next;
            free(entry->file);
            free(entry);
            entry = next;
        }
    }

    free(cov);
}

// Reset all counters
void coverage_reset(coverage_t *cov) {
    if (!cov) return;

    // Reset statement counters
    for (int i = 0; i < COVERAGE_HASH_SIZE; i++) {
        stmt_entry_t *entry = cov->stmt_table[i];
        while (entry) {
            entry->count = 0;
            entry = entry->next;
        }
    }

    // Reset branch counters
    for (int i = 0; i < COVERAGE_HASH_SIZE; i++) {
        branch_entry_t *entry = cov->branch_table[i];
        while (entry) {
            entry->taken_true = 0;
            entry->taken_false = 0;
            entry = entry->next;
        }
    }
}

// Register a statement location
int coverage_register_statement(coverage_t *cov, const char *file, int line) {
    if (!cov || !file) return -1;
    if (cov->stmt_count >= COVERAGE_MAX_ITEMS) return -1;

    uint32_t hash = hash_location(file, line);
    stmt_entry_t *entry = cov->stmt_table[hash];

    // Check if already registered
    while (entry) {
        if (entry->line == line && strcmp(entry->file, file) == 0) {
            return 0; // Already registered
        }
        entry = entry->next;
    }

    // Create new entry
    entry = (stmt_entry_t *)calloc(1, sizeof(stmt_entry_t));
    if (!entry) return -1;

    entry->file = strdup(file);
    entry->line = line;
    entry->count = 0;
    entry->next = cov->stmt_table[hash];
    cov->stmt_table[hash] = entry;
    cov->stmt_count++;

    return 0;
}

// Increment statement execution counter
void coverage_increment_statement(coverage_t *cov, const char *file, int line) {
    if (!cov || !file) return;

    uint32_t hash = hash_location(file, line);
    stmt_entry_t *entry = cov->stmt_table[hash];

    while (entry) {
        if (entry->line == line && strcmp(entry->file, file) == 0) {
            entry->count++;
            return;
        }
        entry = entry->next;
    }

    // Auto-register if not found
    coverage_register_statement(cov, file, line);
    coverage_increment_statement(cov, file, line);
}

// Get statement statistics
int coverage_get_statement_stats(coverage_t *cov, stmt_info_t **stmts, int *count) {
    if (!cov || !stmts || !count) return -1;

    // Allocate array for results
    stmt_info_t *result = (stmt_info_t *)calloc(cov->stmt_count, sizeof(stmt_info_t));
    if (!result) return -1;

    int idx = 0;
    for (int i = 0; i < COVERAGE_HASH_SIZE; i++) {
        stmt_entry_t *entry = cov->stmt_table[i];
        while (entry && idx < cov->stmt_count) {
            result[idx].file = entry->file;
            result[idx].line = entry->line;
            result[idx].count = entry->count;
            idx++;
            entry = entry->next;
        }
    }

    *stmts = result;
    *count = idx;
    return 0;
}

// Print statement coverage report
void coverage_print_statement_report(coverage_t *cov, FILE *fp) {
    if (!cov || !fp) return;

    fprintf(fp, "=== Statement Coverage Report ===\n");

    int total = 0;
    int executed = 0;

    for (int i = 0; i < COVERAGE_HASH_SIZE; i++) {
        stmt_entry_t *entry = cov->stmt_table[i];
        while (entry) {
            total++;
            if (entry->count > 0) {
                executed++;
                fprintf(fp, "%s:%d: executed %u times\n",
                       entry->file, entry->line, entry->count);
            } else {
                fprintf(fp, "%s:%d: NOT EXECUTED\n",
                       entry->file, entry->line);
            }
            entry = entry->next;
        }
    }

    double coverage = total > 0 ? (100.0 * executed / total) : 0.0;
    fprintf(fp, "Statement Coverage: %.1f%% (%d/%d statements executed)\n",
           coverage, executed, total);
}

// Register a branch location
int coverage_register_branch(coverage_t *cov, const char *file, int line) {
    if (!cov || !file) return -1;
    if (cov->branch_count >= COVERAGE_MAX_ITEMS) return -1;

    uint32_t hash = hash_location(file, line);
    branch_entry_t *entry = cov->branch_table[hash];

    // Check if already registered
    while (entry) {
        if (entry->line == line && strcmp(entry->file, file) == 0) {
            return 0; // Already registered
        }
        entry = entry->next;
    }

    // Create new entry
    entry = (branch_entry_t *)calloc(1, sizeof(branch_entry_t));
    if (!entry) return -1;

    entry->file = strdup(file);
    entry->line = line;
    entry->taken_true = 0;
    entry->taken_false = 0;
    entry->next = cov->branch_table[hash];
    cov->branch_table[hash] = entry;
    cov->branch_count++;

    return 0;
}

// Increment branch execution counter
void coverage_increment_branch(coverage_t *cov, const char *file, int line, int taken) {
    if (!cov || !file) return;

    uint32_t hash = hash_location(file, line);
    branch_entry_t *entry = cov->branch_table[hash];

    while (entry) {
        if (entry->line == line && strcmp(entry->file, file) == 0) {
            if (taken) {
                entry->taken_true++;
            } else {
                entry->taken_false++;
            }
            return;
        }
        entry = entry->next;
    }

    // Auto-register if not found
    coverage_register_branch(cov, file, line);
    coverage_increment_branch(cov, file, line, taken);
}

// Get branch statistics
int coverage_get_branch_stats(coverage_t *cov, branch_info_t **branches, int *count) {
    if (!cov || !branches || !count) return -1;

    // Allocate array for results
    branch_info_t *result = (branch_info_t *)calloc(cov->branch_count, sizeof(branch_info_t));
    if (!result) return -1;

    int idx = 0;
    for (int i = 0; i < COVERAGE_HASH_SIZE; i++) {
        branch_entry_t *entry = cov->branch_table[i];
        while (entry && idx < cov->branch_count) {
            result[idx].file = entry->file;
            result[idx].line = entry->line;
            result[idx].taken_true = entry->taken_true;
            result[idx].taken_false = entry->taken_false;
            idx++;
            entry = entry->next;
        }
    }

    *branches = result;
    *count = idx;
    return 0;
}

// Print branch coverage report
void coverage_print_branch_report(coverage_t *cov, FILE *fp) {
    if (!cov || !fp) return;

    fprintf(fp, "=== Branch Coverage Report ===\n");

    int total_branches = 0;
    int covered_branches = 0;
    int partial_branches = 0;

    for (int i = 0; i < COVERAGE_HASH_SIZE; i++) {
        branch_entry_t *entry = cov->branch_table[i];
        while (entry) {
            total_branches++;

            int has_true = entry->taken_true > 0;
            int has_false = entry->taken_false > 0;

            const char *status;
            if (has_true && has_false) {
                status = "COVERED";
                covered_branches++;
            } else if (has_true || has_false) {
                status = "PARTIAL";
                partial_branches++;
            } else {
                status = "NOT COVERED";
            }

            fprintf(fp, "%s:%d: True=%u, False=%u (%s)\n",
                   entry->file, entry->line,
                   entry->taken_true, entry->taken_false,
                   status);

            entry = entry->next;
        }
    }

    double coverage = total_branches > 0 ?
        (100.0 * (covered_branches + partial_branches) / total_branches) : 0.0;
    double full_coverage = total_branches > 0 ?
        (100.0 * covered_branches / total_branches) : 0.0;

    fprintf(fp, "Branch Coverage: %.1f%% (%d/%d branches covered)\n",
           coverage, covered_branches + partial_branches, total_branches);
    fprintf(fp, "Decision Coverage: %.1f%% (%d/%d branches fully covered)\n",
           full_coverage, covered_branches, total_branches);
}

// Print full coverage report
void coverage_print_full_report(coverage_t *cov, FILE *fp) {
    if (!cov || !fp) return;

    fprintf(fp, "=== Coverage Analysis Report ===\n\n");
    coverage_print_statement_report(cov, fp);
    fprintf(fp, "\n");
    coverage_print_branch_report(cov, fp);
}
