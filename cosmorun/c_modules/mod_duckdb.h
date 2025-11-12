#ifndef COSMORUN_DUCKDB_MINIMAL_H
#define COSMORUN_DUCKDB_MINIMAL_H

#include "src/cosmo_libc.h"

/* Override DUCKDB_C_API for TCC compatibility */
#ifndef DUCKDB_C_API
#define DUCKDB_C_API
#endif

/* Define DUCKDB_STATIC_BUILD to avoid __declspec on Windows */
#ifndef DUCKDB_STATIC_BUILD
#define DUCKDB_STATIC_BUILD
#endif

#include "duckdb-1.4.1.h"

typedef duckdb_state (*duckdb_open_t)(const char *path, duckdb_database *out_database);
typedef duckdb_state (*duckdb_connect_t)(duckdb_database database, duckdb_connection *out_connection);
typedef duckdb_state (*duckdb_query_t)(duckdb_connection connection, const char *query, duckdb_result *out_result);
typedef void (*duckdb_destroy_result_t)(duckdb_result *result);
typedef void (*duckdb_disconnect_t)(duckdb_connection *connection);
typedef void (*duckdb_close_t)(duckdb_database *database);
typedef idx_t (*duckdb_row_count_t)(duckdb_result *result);
typedef idx_t (*duckdb_column_count_t)(duckdb_result *result);
typedef const char* (*duckdb_column_name_t)(duckdb_result *result, idx_t col);
typedef char* (*duckdb_value_varchar_t)(duckdb_result *result, idx_t col, idx_t row);
typedef void (*duckdb_free_t)(void *ptr);
typedef const char* (*duckdb_result_error_t)(duckdb_result *result);


#endif /* COSMORUN_DUCKDB_MINIMAL_H */
