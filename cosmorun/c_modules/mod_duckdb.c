#include "mod_duckdb.h"

/* Module initialization - auto-called by __import() */
int mod_duckdb_init(void) {
    /* No dependencies, no initialization needed */
    return 0;  /* Success */
}

static void *duckdb_try_dlopen(const char *path) {
    if (!path || !*path) {
        return NULL;
    }
    return __dlopen(path, 0);
}

static inline void* get_duck_sym(void *handle, const char *name) {
    return __dlsym(handle, name);
}

static int duckdb_run_query_cycle(void *handle, const char *query) {
    duckdb_open_t duckdb_open = (duckdb_open_t)get_duck_sym(handle, "duckdb_open");
    duckdb_connect_t duckdb_connect = (duckdb_connect_t)get_duck_sym(handle, "duckdb_connect");
    duckdb_query_t duckdb_query = (duckdb_query_t)get_duck_sym(handle, "duckdb_query");
    duckdb_destroy_result_t duckdb_destroy_result = (duckdb_destroy_result_t)get_duck_sym(handle, "duckdb_destroy_result");
    duckdb_disconnect_t duckdb_disconnect = (duckdb_disconnect_t)get_duck_sym(handle, "duckdb_disconnect");
    duckdb_close_t duckdb_close = (duckdb_close_t)get_duck_sym(handle, "duckdb_close");
    duckdb_row_count_t duckdb_row_count = (duckdb_row_count_t)get_duck_sym(handle, "duckdb_row_count");
    duckdb_column_count_t duckdb_column_count = (duckdb_column_count_t)get_duck_sym(handle, "duckdb_column_count");
    duckdb_column_name_t duckdb_column_name = (duckdb_column_name_t)get_duck_sym(handle, "duckdb_column_name");

    if (!duckdb_open || !duckdb_connect || !duckdb_query) {
        printf("✗ Failed to resolve essential DuckDB functions\n");
        return -1;
    }

    duckdb_database database = NULL;
    duckdb_connection connection = NULL;

    printf("Opening in-memory DuckDB database...\n");
    if (duckdb_open(NULL, &database) != DuckDBSuccess || !database) {
        printf("✗ duckdb_open failed\n");
        return -1;
    }
    printf("✓ Database handle: %p\n", database);

    printf("Creating connection...\n");
    if (duckdb_connect(database, &connection) != DuckDBSuccess || !connection) {
        printf("✗ duckdb_connect failed\n");
        if (duckdb_close) {
            duckdb_close(&database);
        }
        return -1;
    }
    printf("✓ Connection handle: %p\n", connection);

    duckdb_result result = {0};
    printf("Executing query: %s\n", query);
    if (duckdb_query(connection, query, &result) != DuckDBSuccess) {
        printf("✗ duckdb_query failed\n");
        if (duckdb_disconnect) {
            duckdb_disconnect(&connection);
        }
        if (duckdb_close) {
            duckdb_close(&database);
        }
        return -1;
    }
    printf("✓ Query completed\n");

    if (duckdb_row_count && duckdb_column_count) {
        idx_t rows = duckdb_row_count(&result);
        idx_t cols = duckdb_column_count(&result);
        printf("Result set: %llu rows, %llu columns\n", rows, cols);
        if (duckdb_column_name) {
            printf("Columns: ");
            for (idx_t i = 0; i < cols; i++) {
                const char *name = duckdb_column_name(&result, i);
                printf("'%s' ", name ? name : "NULL");
            }
            printf("\n");
        }
    }

    if (duckdb_destroy_result) {
        printf("Destroying result...\n");
        duckdb_destroy_result(&result);
        printf("✓ Result destroyed\n");
    }

    if (duckdb_disconnect) {
        duckdb_disconnect(&connection);
        printf("✓ Connection closed\n");
    }

    if (duckdb_close) {
        duckdb_close(&database);
        printf("✓ Database closed\n");
    }

    return 0;
}

static void *duckdb_dlopen_auto(const char *requested_path) {
    static const char *const candidates[] = {
#if defined(_WIN32) || defined(_WIN64)
        "lib/libduckdb.dll",
        "../lib/duckdb.dll",
        "lib/duckdb.dll",
        "./duckdb.dll",
        "duckdb.dll",
        "./libduckdb.dll",
        "libduckdb.dll",
#elif defined(__APPLE__)
        "lib/libduckdb.dylib",
        "../lib/libduckdb.dylib",
        "./libduckdb.dylib",
        "libduckdb.dylib",
#else
        "lib/libduckdb.so",
        "../lib/libduckdb.so",
        "./libduckdb.so",
        "libduckdb.so",
        "./duckdb.so",
        "duckdb.so",
#endif
        NULL
    };

    if (requested_path && *requested_path) {
        void *handle = duckdb_try_dlopen(requested_path);
        if (handle) {
            return handle;
        }
    }

    for (const char *const *cand = candidates; *cand; ++cand) {
        if (requested_path && *requested_path && strcmp(requested_path, *cand) == 0) {
            continue;
        }
        void *handle = duckdb_try_dlopen(*cand);
        if (handle) {
            return handle;
        }
    }
    return NULL;
}

int duckdb_selftest(const char *lib_path) {
    const char *path = (lib_path && *lib_path) ? lib_path : "";
    printf("=== DuckDB Self Test (library hint: %s) ===\n", path[0] ? path : "<auto>");

    void *handle = duckdb_dlopen_auto(path);
    if (!handle) {
        char *error = dlerror();
        printf("✗ Failed to load DuckDB shared library: %s\n", error ? error : "unknown error");
        return -1;
    }
    printf("✓ Loaded library handle: %p\n", handle);

    int rc = duckdb_run_query_cycle(handle, "SELECT 42 AS answer, 'Hello' AS greeting");

    dlclose(handle);
    return rc;
}

int duckdb_selftest_default(void) {
    return duckdb_selftest(NULL);
}

typedef struct {
    void *handle;
    duckdb_open_t open_fn;
    duckdb_close_t close_fn;
    duckdb_connect_t connect_fn;
    duckdb_disconnect_t disconnect_fn;
    duckdb_query_t query_fn;
    duckdb_destroy_result_t destroy_result_fn;
    duckdb_row_count_t row_count_fn;
    duckdb_column_count_t column_count_fn;
    duckdb_value_varchar_t value_varchar_fn;
    duckdb_free_t free_fn;
    duckdb_result_error_t result_error_fn;
} duckdb_runtime_t;

extern int unlink(const char *pathname);
extern int access(const char *pathname, int mode);

static int resolve_runtime(duckdb_runtime_t *rt, const char *hint) {
    void *handle = duckdb_dlopen_auto(hint);
    if (!handle) {
        char *err = dlerror();
        printf("✗ Failed to load DuckDB shared library: %s\n", err ? err : "unknown error");
        return -1;
    }
    rt->handle = handle;
    rt->open_fn = (duckdb_open_t)get_duck_sym(handle, "duckdb_open");
    rt->close_fn = (duckdb_close_t)get_duck_sym(handle, "duckdb_close");
    rt->connect_fn = (duckdb_connect_t)get_duck_sym(handle, "duckdb_connect");
    rt->disconnect_fn = (duckdb_disconnect_t)get_duck_sym(handle, "duckdb_disconnect");
    rt->query_fn = (duckdb_query_t)get_duck_sym(handle, "duckdb_query");
    rt->destroy_result_fn = (duckdb_destroy_result_t)get_duck_sym(handle, "duckdb_destroy_result");
    rt->row_count_fn = (duckdb_row_count_t)get_duck_sym(handle, "duckdb_row_count");
    rt->column_count_fn = (duckdb_column_count_t)get_duck_sym(handle, "duckdb_column_count");
    rt->value_varchar_fn = (duckdb_value_varchar_t)get_duck_sym(handle, "duckdb_value_varchar");
    rt->free_fn = (duckdb_free_t)get_duck_sym(handle, "duckdb_free");
    rt->result_error_fn = (duckdb_result_error_t)get_duck_sym(handle, "duckdb_result_error");
    if (!rt->open_fn || !rt->close_fn || !rt->connect_fn || !rt->disconnect_fn ||
        !rt->query_fn || !rt->destroy_result_fn || !rt->row_count_fn ||
        !rt->column_count_fn || !rt->value_varchar_fn || !rt->free_fn ||
        !rt->result_error_fn) {
        printf("✗ DuckDB runtime missing required symbols\n");
        dlclose(handle);
        rt->handle = NULL;
        return -1;
    }
    return 0;
}

static void runtime_close(duckdb_runtime_t *rt) {
    if (rt->handle) {
        dlclose(rt->handle);
        rt->handle = NULL;
    }
}

static int runtime_exec_sql(duckdb_runtime_t *rt, duckdb_connection conn, const char *sql, const char *tag) {
    duckdb_result result = {0};
    duckdb_state state = rt->query_fn(conn, sql, &result);
    if (state != DuckDBSuccess) {
        const char *err = rt->result_error_fn(&result);
        printf("✗ %s failed: %s\n", tag, err ? err : "unknown error");
        rt->destroy_result_fn(&result);
        return -1;
    }
    rt->destroy_result_fn(&result);
    printf("✓ %s\n", tag);
    return 0;
}

static long parse_long(const char *s) {
    long sign = 1;
    long value = 0;
    if (!s) return 0;
    if (*s == '-') {
        sign = -1;
        ++s;
    }
    while (*s >= '0' && *s <= '9') {
        value = value * 10 + (*s - '0');
        ++s;
    }
    return sign * value;
}

int duckdb_arrow_roundtrip(const char *arrow_path) {
    duckdb_runtime_t rt = {0};
    duckdb_database db = NULL;
    duckdb_connection conn = NULL;
    int rc = -1;
    const char *path = arrow_path && *arrow_path ? arrow_path : "duckdb_arrow_test.arrow";

    unlink(path);

    if (resolve_runtime(&rt, NULL) != 0) {
        return -1;
    }
    if (rt.open_fn(NULL, &db) != DuckDBSuccess || !db) {
        printf("✗ duckdb_open failed\n");
        runtime_close(&rt);
        return -1;
    }
    if (rt.connect_fn(db, &conn) != DuckDBSuccess || !conn) {
        printf("✗ duckdb_connect failed\n");
        rt.close_fn(&db);
        runtime_close(&rt);
        return -1;
    }

    runtime_exec_sql(&rt, conn, "SET home_directory='.';", "set home directory");
    runtime_exec_sql(&rt, conn, "INSTALL arrow;", "install arrow extension");
    runtime_exec_sql(&rt, conn, "LOAD arrow;", "load arrow extension");

    if (runtime_exec_sql(&rt, conn,
                         "CREATE TABLE trades_source(symbol VARCHAR, price DOUBLE, volume BIGINT);",
                         "create trades_source") != 0) {
        goto cleanup;
    }
    if (runtime_exec_sql(&rt, conn,
                         "INSERT INTO trades_source VALUES "
                         "('AAPL', 150.25, 1000000),"
                         "('GOOGL', 2810.50, 650000),"
                         "('TSLA', 705.10, 2400000);",
                         "insert sample rows") != 0) {
        goto cleanup;
    }
    char export_sql[256];
    char import_sql[256];
    {
        const char *prefix = "COPY trades_source TO '";
        const char *suffix = "' (FORMAT 'arrow');";
        int pos = 0;
        while (prefix[pos]) {
            export_sql[pos] = prefix[pos];
            pos++;
        }
        int i = 0;
        while (path[i] && pos + 1 < 255) {
            export_sql[pos++] = path[i++];
        }
        int j = 0;
        while (suffix[j] && pos + 1 < 255) {
            export_sql[pos++] = suffix[j++];
        }
        export_sql[pos] = '\0';
    }

    if (runtime_exec_sql(&rt, conn, export_sql, "export arrow file") != 0) {
        goto cleanup;
    }
    if (access(path, 0) != 0) {
        printf("✗ arrow file not found after export\n");
        goto cleanup;
    }
    if (runtime_exec_sql(&rt, conn,
                         "CREATE TABLE trades_import(symbol VARCHAR, price DOUBLE, volume BIGINT);",
                         "create trades_import") != 0) {
        goto cleanup;
    }
    {
        const char *prefix = "COPY trades_import FROM '";
        const char *suffix = "' (FORMAT 'arrow');";
        int pos = 0;
        while (prefix[pos]) {
            import_sql[pos] = prefix[pos];
            pos++;
        }
        int i = 0;
        while (path[i] && pos + 1 < 255) {
            import_sql[pos++] = path[i++];
        }
        int j = 0;
        while (suffix[j] && pos + 1 < 255) {
            import_sql[pos++] = suffix[j++];
        }
        import_sql[pos] = '\0';
    }

    if (runtime_exec_sql(&rt, conn, import_sql, "import arrow file") != 0) {
        goto cleanup;
    }

    {
        duckdb_result result = {0};
        if (rt.query_fn(conn,
                        "SELECT COUNT(*) AS cnt, SUM(volume) AS total_volume FROM trades_import;",
                        &result) != DuckDBSuccess) {
            const char *err = rt.result_error_fn(&result);
            printf("✗ verification query failed: %s\n", err ? err : "unknown error");
            rt.destroy_result_fn(&result);
            goto cleanup;
        }
        if (rt.row_count_fn(&result) > 0 && rt.column_count_fn(&result) >= 2) {
            char *cnt_str = rt.value_varchar_fn(&result, 0, 0);
            char *sum_str = rt.value_varchar_fn(&result, 1, 0);
            long count = parse_long(cnt_str);
            long total = parse_long(sum_str);
            if (cnt_str) rt.free_fn(cnt_str);
            if (sum_str) rt.free_fn(sum_str);
            printf("Imported rows=%ld total_volume=%ld\n", count, total);
            if (count == 3 && total == (1000000 + 650000 + 2400000)) {
                printf("✓ Arrow round-trip verification passed\n");
                rc = 0;
            } else {
                printf("✗ verification mismatch\n");
            }
        } else {
            printf("✗ verification query returned no data\n");
        }
        rt.destroy_result_fn(&result);
    }

cleanup:
    runtime_exec_sql(&rt, conn, "DROP TABLE IF EXISTS trades_source;", "drop trades_source");
    runtime_exec_sql(&rt, conn, "DROP TABLE IF EXISTS trades_import;", "drop trades_import");
    if (conn) {
        rt.disconnect_fn(&conn);
    }
    if (db) {
        rt.close_fn(&db);
    }
    runtime_close(&rt);
    unlink(path);
    return rc;
}

// ============================================================================
// Public API: DuckDB Context for external modules
// ============================================================================

typedef struct {
    void *lib_handle;
    duckdb_database database;
    duckdb_connection connection;

    // Function pointers
    duckdb_open_t open_fn;
    duckdb_connect_t connect_fn;
    duckdb_query_t query_fn;
    duckdb_close_t close_fn;
    duckdb_disconnect_t disconnect_fn;
    duckdb_destroy_result_t destroy_result_fn;
    duckdb_row_count_t row_count_fn;
    duckdb_column_count_t column_count_fn;
    duckdb_column_name_t column_name_fn;
    duckdb_value_varchar_t value_varchar_fn;
    duckdb_free_t free_fn;
    duckdb_result_error_t result_error_fn;
} duckdb_context_t;

/**
 * Initialize DuckDB context and load library
 * @param lib_path Optional library path (NULL for auto-detection)
 * @return Pointer to allocated context, or NULL on failure
 */
duckdb_context_t* duckdb_init(const char *lib_path) {
    void *handle = duckdb_dlopen_auto(lib_path ? lib_path : "");
    if (!handle) {
        return NULL;
    }

    duckdb_context_t *ctx = (duckdb_context_t*)malloc(sizeof(duckdb_context_t));
    if (!ctx) {
        dlclose(handle);
        return NULL;
    }

    ctx->lib_handle = handle;
    ctx->database = NULL;
    ctx->connection = NULL;

    // Load function pointers (with Windows calling convention conversion)
    ctx->open_fn = (duckdb_open_t)get_duck_sym(handle, "duckdb_open");
    ctx->connect_fn = (duckdb_connect_t)get_duck_sym(handle, "duckdb_connect");
    ctx->query_fn = (duckdb_query_t)get_duck_sym(handle, "duckdb_query");
    ctx->close_fn = (duckdb_close_t)get_duck_sym(handle, "duckdb_close");
    ctx->disconnect_fn = (duckdb_disconnect_t)get_duck_sym(handle, "duckdb_disconnect");
    ctx->destroy_result_fn = (duckdb_destroy_result_t)get_duck_sym(handle, "duckdb_destroy_result");
    ctx->row_count_fn = (duckdb_row_count_t)get_duck_sym(handle, "duckdb_row_count");
    ctx->column_count_fn = (duckdb_column_count_t)get_duck_sym(handle, "duckdb_column_count");
    ctx->column_name_fn = (duckdb_column_name_t)get_duck_sym(handle, "duckdb_column_name");
    ctx->value_varchar_fn = (duckdb_value_varchar_t)get_duck_sym(handle, "duckdb_value_varchar");
    ctx->free_fn = (duckdb_free_t)get_duck_sym(handle, "duckdb_free");
    ctx->result_error_fn = (duckdb_result_error_t)get_duck_sym(handle, "duckdb_result_error");

    if (!ctx->open_fn || !ctx->connect_fn || !ctx->query_fn) {
        free(ctx);
        dlclose(handle);
        return NULL;
    }

    return ctx;
}

/**
 * Open database and create connection
 * @param ctx DuckDB context
 * @param db_path Database path (":memory:" for in-memory, NULL for ":memory:")
 * @return 0 on success, -1 on failure
 */
int duckdb_open_db(duckdb_context_t *ctx, const char *db_path) {
    if (!ctx) return -1;

    const char *path = db_path ? db_path : ":memory:";
    if (ctx->open_fn(path, &ctx->database) != DuckDBSuccess || !ctx->database) {
        return -1;
    }

    if (ctx->connect_fn(ctx->database, &ctx->connection) != DuckDBSuccess || !ctx->connection) {
        if (ctx->close_fn) {
            ctx->close_fn(&ctx->database);
        }
        ctx->database = NULL;
        return -1;
    }

    return 0;
}

/**
 * Execute query and return result
 * @param ctx DuckDB context
 * @param sql SQL query string
 * @param out_result Pointer to result structure (caller must destroy it)
 * @return 0 on success, -1 on failure
 */
int duckdb_exec(duckdb_context_t *ctx, const char *sql, duckdb_result *out_result) {
    if (!ctx || !ctx->connection || !sql || !out_result) return -1;

    if (ctx->query_fn(ctx->connection, sql, out_result) != DuckDBSuccess) {
        return -1;
    }

    return 0;
}

/**
 * Get row count from result
 */
long duckdb_get_row_count(duckdb_context_t *ctx, duckdb_result *result) {
    if (!ctx || !result || !ctx->row_count_fn) return 0;
    return (long)ctx->row_count_fn(result);
}

/**
 * Get column count from result
 */
long duckdb_get_column_count(duckdb_context_t *ctx, duckdb_result *result) {
    if (!ctx || !result || !ctx->column_count_fn) return 0;
    return (long)ctx->column_count_fn(result);
}

/**
 * Get column name from result
 */
const char* duckdb_get_column_name(duckdb_context_t *ctx, duckdb_result *result, long col) {
    if (!ctx || !result || !ctx->column_name_fn) return NULL;
    return ctx->column_name_fn(result, (idx_t)col);
}

/**
 * Get varchar value from result
 * @return Allocated string (caller must free with duckdb_free_value)
 * Note: Renamed from duckdb_get_varchar to avoid conflict with official API
 */
char* duckdb_ctx_varchar(duckdb_context_t *ctx, duckdb_result *result, long col, long row) {
    if (!ctx || !result || !ctx->value_varchar_fn) return NULL;
    return ctx->value_varchar_fn(result, (idx_t)col, (idx_t)row);
}

/**
 * Free value returned by DuckDB
 */
void duckdb_free_value(duckdb_context_t *ctx, void *value) {
    if (ctx && ctx->free_fn && value) {
        ctx->free_fn(value);
    }
}

/**
 * Destroy query result (wrapper to avoid name conflict with duckdb.h)
 */
void duckdb_free_result(duckdb_context_t *ctx, duckdb_result *result) {
    if (ctx && ctx->destroy_result_fn && result) {
        ctx->destroy_result_fn(result);
    }
}

/**
 * Get error message from result
 */
const char* duckdb_get_error(duckdb_context_t *ctx, duckdb_result *result) {
    if (!ctx || !result || !ctx->result_error_fn) return NULL;
    return ctx->result_error_fn(result);
}

/**
 * Close connection and database
 */
void duckdb_close_db(duckdb_context_t *ctx) {
    if (!ctx) return;

    if (ctx->connection && ctx->disconnect_fn) {
        ctx->disconnect_fn(&ctx->connection);
        ctx->connection = NULL;
    }

    if (ctx->database && ctx->close_fn) {
        ctx->close_fn(&ctx->database);
        ctx->database = NULL;
    }
}

/**
 * Cleanup DuckDB context and free resources
 */
void duckdb_cleanup(duckdb_context_t *ctx) {
    if (!ctx) return;

    duckdb_close_db(ctx);

    if (ctx->lib_handle) {
        dlclose(ctx->lib_handle);
        ctx->lib_handle = NULL;
    }

    free(ctx);
}
