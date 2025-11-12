/*
 * example_sqlite.c - SQLite Database Examples
 *
 * Demonstrates:
 * - Opening/closing databases
 * - Creating tables
 * - INSERT/SELECT/UPDATE/DELETE operations
 * - Prepared statements
 * - Error handling
 */

#include "src/cosmo_libc.h"
#include "c_modules/mod_sqlite3.c"

void example_basic_operations() {
    printf("\n========================================\n");
    printf("  Basic SQLite Operations\n");
    printf("========================================\n\n");

    /* Example 1: Open in-memory database */
    printf("1. Open In-Memory Database:\n");
    sqlite3 *db = NULL;
    int rc = sqlite3_open_ptr(":memory:", &db);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "   ERROR: Cannot open database\n");
        return;
    }
    printf("   Database opened successfully\n\n");

    /* Example 2: Create table */
    printf("2. Create Table:\n");
    const char *create_sql =
        "CREATE TABLE users ("
        "  id INTEGER PRIMARY KEY,"
        "  name TEXT NOT NULL,"
        "  email TEXT,"
        "  age INTEGER"
        ");";

    char *err_msg = NULL;
    rc = sqlite3_exec_ptr(db, create_sql, NULL, NULL, &err_msg);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "   ERROR: %s\n", err_msg);
        if (err_msg) free(err_msg);
        sqlite3_close_ptr(db);
        return;
    }
    printf("   Table 'users' created\n\n");

    /* Example 3: Insert data */
    printf("3. Insert Data:\n");
    const char *insert_sql =
        "INSERT INTO users (name, email, age) VALUES "
        "('Alice', 'alice@example.com', 30),"
        "('Bob', 'bob@example.com', 25),"
        "('Charlie', 'charlie@example.com', 35);";

    rc = sqlite3_exec_ptr(db, insert_sql, NULL, NULL, &err_msg);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "   ERROR: %s\n", err_msg);
        if (err_msg) free(err_msg);
    } else {
        int changes = sqlite3_changes_ptr(db);
        printf("   Inserted %d rows\n", changes);
    }
    printf("\n");

    /* Example 4: Query data */
    printf("4. Query Data:\n");
    sqlite3_stmt *stmt = NULL;
    const char *select_sql = "SELECT id, name, email, age FROM users WHERE age >= 30;";

    rc = sqlite3_prepare_v2_ptr(db, select_sql, -1, &stmt, NULL);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "   ERROR: %s\n", sqlite3_errmsg_ptr(db));
    } else {
        printf("   Users with age >= 30:\n");
        while (sqlite3_step_ptr(stmt) == SQLITE_ROW) {
            int id = sqlite3_column_int_ptr(stmt, 0);
            const unsigned char *name = sqlite3_column_text_ptr(stmt, 1);
            const unsigned char *email = sqlite3_column_text_ptr(stmt, 2);
            int age = sqlite3_column_int_ptr(stmt, 3);

            printf("     ID=%d, Name=%s, Email=%s, Age=%d\n",
                   id, name, email, age);
        }
        sqlite3_finalize_ptr(stmt);
    }
    printf("\n");

    /* Close database */
    sqlite3_close_ptr(db);
    printf("   Database closed\n");
}

void example_prepared_statements() {
    printf("\n========================================\n");
    printf("  Prepared Statements\n");
    printf("========================================\n\n");

    sqlite3 *db = NULL;
    int rc = sqlite3_open_ptr(":memory:", &db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "   ERROR: Cannot open database\n");
        return;
    }

    /* Create table */
    char *err_msg = NULL;
    sqlite3_exec_ptr(db, "CREATE TABLE products (id INTEGER PRIMARY KEY, name TEXT, price REAL);",
                     NULL, NULL, &err_msg);

    /* Example: Insert with prepared statement */
    printf("1. Insert with Prepared Statement:\n");
    sqlite3_stmt *stmt = NULL;

    /* Insert laptop */
    const char *insert1 = "INSERT INTO products (name, price) VALUES (?, ?);";
    sqlite3_prepare_v2_ptr(db, insert1, -1, &stmt, NULL);
    sqlite3_bind_text_ptr(stmt, 1, "Laptop", -1, NULL);
    sqlite3_bind_double_ptr(stmt, 2, 999.99);
    sqlite3_step_ptr(stmt);
    sqlite3_finalize_ptr(stmt);

    /* Insert mouse */
    sqlite3_prepare_v2_ptr(db, insert1, -1, &stmt, NULL);
    sqlite3_bind_text_ptr(stmt, 1, "Mouse", -1, NULL);
    sqlite3_bind_double_ptr(stmt, 2, 29.99);
    sqlite3_step_ptr(stmt);
    sqlite3_finalize_ptr(stmt);

    /* Insert keyboard */
    sqlite3_prepare_v2_ptr(db, insert1, -1, &stmt, NULL);
    sqlite3_bind_text_ptr(stmt, 1, "Keyboard", -1, NULL);
    sqlite3_bind_double_ptr(stmt, 2, 79.99);
    sqlite3_step_ptr(stmt);
    sqlite3_finalize_ptr(stmt);

    printf("   Inserted 3 products using prepared statements\n");
    printf("\n");

    /* Example: Query with parameter binding */
    printf("2. Query with Parameter Binding:\n");
    const char *query_sql = "SELECT name, price FROM products WHERE price < ?;";

    rc = sqlite3_prepare_v2_ptr(db, query_sql, -1, &stmt, NULL);

    if (rc == SQLITE_OK) {
        sqlite3_bind_double_ptr(stmt, 1, 100.0);  /* price < 100 */

        printf("   Products under $100:\n");
        while (sqlite3_step_ptr(stmt) == SQLITE_ROW) {
            const unsigned char *name = sqlite3_column_text_ptr(stmt, 0);
            double price = sqlite3_column_double_ptr(stmt, 1);
            printf("     %s - $%.2f\n", name, price);
        }

        sqlite3_finalize_ptr(stmt);
    }
    printf("\n");

    sqlite3_close_ptr(db);
}

int main(void) {
    printf("\n");
    printf("╔════════════════════════════════════════╗\n");
    printf("║  CosmoRun SQLite Module Examples      ║\n");
    printf("╚════════════════════════════════════════╝\n");

    /* Check if SQLite is available */
    if (!sqlite3_lib_handle) {
        fprintf(stderr, "\nSQLite library not loaded - install libsqlite3 to use this module\n\n");
        return 1;
    }

    example_basic_operations();
    example_prepared_statements();

    printf("========================================\n");
    printf("  All SQLite examples completed!\n");
    printf("========================================\n\n");

    return 0;
}
