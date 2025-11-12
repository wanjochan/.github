/**
 * @file cosmo_analyzer.h
 * @brief Static Code Analysis Tool
 *
 * Provides basic static analysis for C code to catch common bugs:
 * - Dead code detection (unused functions, unreachable code)
 * - Unused variables (declared but never read)
 * - Type safety warnings (implicit conversions, type mismatches)
 */

#ifndef COSMO_ANALYZER_H
#define COSMO_ANALYZER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Analysis result flags */
#define ANALYZE_OK              0
#define ANALYZE_ERROR_FILE      -1
#define ANALYZE_ERROR_PARSE     -2
#define ANALYZE_ERROR_MEMORY    -3

/* Analysis options */
typedef struct {
    int verbose;              /* Enable verbose output */
    int check_dead_code;      /* Check for dead code (unused functions) */
    int check_unused_vars;    /* Check for unused variables (file-scope) */
    int check_type_safety;    /* Check for type safety issues */
    int check_null_deref;     /* Check for potential NULL dereferences */
    int check_unreachable;    /* Check for unreachable code after return/exit */
    int check_memory_leaks;   /* Check for basic memory leaks (malloc without free) */
    int check_local_unused;   /* Check for unused local variables */
    int check_uninitialized;  /* Check for uninitialized variables */
} AnalysisOptions;

/* Analysis issue types */
typedef enum {
    ISSUE_DEAD_CODE,
    ISSUE_UNUSED_VAR,
    ISSUE_TYPE_SAFETY,
    ISSUE_NULL_DEREF,
    ISSUE_UNREACHABLE_CODE,
    ISSUE_MEMORY_LEAK,
    ISSUE_LOCAL_UNUSED,
    ISSUE_UNINITIALIZED
} IssueType;

/* Issue severity */
typedef enum {
    SEVERITY_WARNING,
    SEVERITY_ERROR,
    SEVERITY_INFO
} IssueSeverity;

/* Analysis issue */
typedef struct AnalysisIssue {
    IssueType type;
    IssueSeverity severity;
    const char *file;
    int line;
    int column;
    char message[256];
    struct AnalysisIssue *next;
} AnalysisIssue;

/* Analysis result */
typedef struct {
    int total_issues;
    int error_count;
    int warning_count;
    int info_count;
    AnalysisIssue *issues;
} AnalysisResult;

/**
 * Analyze C source file for common issues
 * @param file Source file path
 * @param options Analysis options
 * @param result Output analysis result (caller must free with free_analysis_result)
 * @return 0 on success, negative on error
 */
int analyze_file(const char *file, const AnalysisOptions *options, AnalysisResult *result);

/**
 * Print analysis report to stdout
 * @param result Analysis result
 * @param file Source file analyzed
 */
void print_analysis_report(const AnalysisResult *result, const char *file);

/**
 * Free analysis result
 * @param result Analysis result to free
 */
void free_analysis_result(AnalysisResult *result);

/**
 * Initialize default analysis options
 * @param options Options structure to initialize
 */
void init_default_analysis_options(AnalysisOptions *options);

#ifdef __cplusplus
}
#endif

#endif /* COSMO_ANALYZER_H */
