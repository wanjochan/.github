/* cosmo_coverage.h - Coverage tracking for cosmorun
 * Provides statement and branch coverage instrumentation
 */

#ifndef COSMO_COVERAGE_H
#define COSMO_COVERAGE_H

#include <stdint.h>
#include <stdio.h>

// Forward declarations
typedef struct coverage_t coverage_t;

// Branch information structure
typedef struct {
    const char *file;
    int line;
    uint32_t taken_true;
    uint32_t taken_false;
} branch_info_t;

// Statement information structure
typedef struct {
    const char *file;
    int line;
    uint32_t count;
} stmt_info_t;

// Coverage API - Lifecycle
coverage_t *coverage_create(void);
void coverage_destroy(coverage_t *cov);
void coverage_reset(coverage_t *cov);

// Statement coverage API
int coverage_register_statement(coverage_t *cov, const char *file, int line);
void coverage_increment_statement(coverage_t *cov, const char *file, int line);
int coverage_get_statement_stats(coverage_t *cov, stmt_info_t **stmts, int *count);
void coverage_print_statement_report(coverage_t *cov, FILE *fp);

// Branch coverage API
int coverage_register_branch(coverage_t *cov, const char *file, int line);
void coverage_increment_branch(coverage_t *cov, const char *file, int line, int taken);
int coverage_get_branch_stats(coverage_t *cov, branch_info_t **branches, int *count);
void coverage_print_branch_report(coverage_t *cov, FILE *fp);

// Combined coverage report
void coverage_print_full_report(coverage_t *cov, FILE *fp);

// Global coverage instance (for instrumented code)
extern coverage_t *g_coverage;

#endif // COSMO_COVERAGE_H
