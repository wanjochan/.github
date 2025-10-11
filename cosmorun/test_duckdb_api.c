#include "cosmo_libc.h"

//#define NULL ((void*)0)
//extern int printf(const char *format, ...);

extern void *__import(const char *path);
extern void *__import_sym(void *module, const char *symbol);

// Forward declarations for DuckDB types
typedef void* duckdb_database;
typedef void* duckdb_connection;
typedef unsigned long long idx_t;
typedef void* duckdb_column;
typedef struct {
    idx_t __deprecated_column_count;
    idx_t __deprecated_row_count;
    idx_t __deprecated_rows_changed;
    duckdb_column *__deprecated_columns;
    char *__deprecated_error_message;
    void *internal_data;
} duckdb_result;

// DuckDB context type from duckdb.c
typedef struct {
    void *lib_handle;
    duckdb_database database;
    duckdb_connection connection;
    void *open_fn;
    void *connect_fn;
    void *query_fn;
    void *close_fn;
    void *disconnect_fn;
    void *destroy_result_fn;
    void *row_count_fn;
    void *column_count_fn;
    void *column_name_fn;
    void *value_varchar_fn;
    void *free_fn;
    void *result_error_fn;
} duckdb_context_t;

int main(void) {
    printf("=== Testing DuckDB Context API ===\n\n");

    // Import duckdb.c module
    void *duckdb_module = __import("mod_duckdb.c");
    if (!duckdb_module) {
        printf("✗ Failed to import duckdb.c\n");
        return 1;
    }
    printf("✓ duckdb.c module loaded\n");

    // Get API functions
    duckdb_context_t* (*duckdb_init)(const char*) =
        (duckdb_context_t*(*)(const char*))__import_sym(duckdb_module, "duckdb_init");
    int (*duckdb_open_db)(duckdb_context_t*, const char*) =
        (int(*)(duckdb_context_t*, const char*))__import_sym(duckdb_module, "duckdb_open_db");
    int (*duckdb_exec)(duckdb_context_t*, const char*, duckdb_result*) =
        (int(*)(duckdb_context_t*, const char*, duckdb_result*))__import_sym(duckdb_module, "duckdb_exec");
    long (*duckdb_get_row_count)(duckdb_context_t*, duckdb_result*) =
        (long(*)(duckdb_context_t*, duckdb_result*))__import_sym(duckdb_module, "duckdb_get_row_count");
    long (*duckdb_get_column_count)(duckdb_context_t*, duckdb_result*) =
        (long(*)(duckdb_context_t*, duckdb_result*))__import_sym(duckdb_module, "duckdb_get_column_count");
    const char* (*duckdb_get_column_name)(duckdb_context_t*, duckdb_result*, long) =
        (const char*(*)(duckdb_context_t*, duckdb_result*, long))__import_sym(duckdb_module, "duckdb_get_column_name");
    char* (*duckdb_ctx_varchar)(duckdb_context_t*, duckdb_result*, long, long) =
        (char*(*)(duckdb_context_t*, duckdb_result*, long, long))__import_sym(duckdb_module, "duckdb_ctx_varchar");
    void (*duckdb_free_value)(duckdb_context_t*, void*) =
        (void(*)(duckdb_context_t*, void*))__import_sym(duckdb_module, "duckdb_free_value");
    void (*duckdb_free_result)(duckdb_context_t*, duckdb_result*) =
        (void(*)(duckdb_context_t*, duckdb_result*))__import_sym(duckdb_module, "duckdb_free_result");
    void (*duckdb_close_db)(duckdb_context_t*) =
        (void(*)(duckdb_context_t*))__import_sym(duckdb_module, "duckdb_close_db");
    void (*duckdb_cleanup)(duckdb_context_t*) =
        (void(*)(duckdb_context_t*))__import_sym(duckdb_module, "duckdb_cleanup");

    if (!duckdb_init || !duckdb_open_db || !duckdb_exec) {
        printf("✗ Failed to resolve DuckDB API functions\n");
        return 1;
    }
    printf("✓ DuckDB API functions resolved\n\n");

    // Initialize DuckDB context
    printf("Initializing DuckDB context...\n");
    duckdb_context_t *ctx = duckdb_init(NULL);
    if (!ctx) {
        printf("✗ Failed to initialize DuckDB context\n");
        return 1;
    }
    printf("✓ DuckDB context initialized\n");

    // Open database
    printf("Opening in-memory database...\n");
    if (duckdb_open_db(ctx, ":memory:") != 0) {
        printf("✗ Failed to open database\n");
        duckdb_cleanup(ctx);
        return 1;
    }
    printf("✓ Database opened\n");

    // Execute test query
    printf("Executing test query...\n");
    duckdb_result result = {0};
    if (duckdb_exec(ctx, "SELECT 42 AS answer, 'Hello DuckDB' AS greeting", &result) != 0) {
        printf("✗ Query failed\n");
        duckdb_close_db(ctx);
        duckdb_cleanup(ctx);
        return 1;
    }
    printf("✓ Query executed\n");

    // Get result metadata
    long rows = duckdb_get_row_count(ctx, &result);
    long cols = duckdb_get_column_count(ctx, &result);
    printf("Result: %ld rows, %ld columns\n", rows, cols);

    // Print column names
    printf("Cols:");
    long i;
    for (i = 0; i < cols; i++) {
        const char *name = duckdb_get_column_name(ctx, &result, i);
        printf("\t%s", name ? name : "NULL");
    }
    printf("\n");

    // Print first row
    if (rows > 0) {
        printf("Rows:");
        for (i = 0; i < cols; i++) {
            char *value = duckdb_ctx_varchar(ctx, &result, i, 0);
            printf("\t%s", value ? value : "NULL");
            if (value) {
                duckdb_free_value(ctx, value);
            }
        }
        printf("\n");
    }

    // Cleanup
    duckdb_free_result(ctx, &result);
    duckdb_close_db(ctx);
    duckdb_cleanup(ctx);

    printf("\n✓ All tests passed!\n");
    return 0;
}
