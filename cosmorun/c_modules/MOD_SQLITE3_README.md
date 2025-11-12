# mod_sqlite3 - SQLite Database Module

**File**: `c_modules/mod_sqlite3.c`
**Dependencies**: None (dynamically loads libsqlite3)
**External libs**: `libsqlite3` (dynamically loaded)

## Overview

`mod_sqlite3` provides full SQLite3 database functionality through dynamic library loading. It supports all standard SQL operations including transactions, prepared statements, and full-text search. The module gracefully handles missing library scenarios.

## Quick Start

```c
#include "mod_sqlite3.c"

sqlite3 *db;
int rc = sqlite3_open_ptr(":memory:", &db);

if (rc == SQLITE_OK) {
    sqlite3_exec_ptr(db, "CREATE TABLE users (id INT, name TEXT);", NULL, NULL, NULL);
    sqlite3_exec_ptr(db, "INSERT INTO users VALUES (1, 'Alice');", NULL, NULL, NULL);

    sqlite3_close_ptr(db);
}
```

## Common Use Cases

### 1. Create and Query Table

```c
sqlite3 *db;
sqlite3_open_ptr("mydata.db", &db);

// Create table
const char *create_sql =
    "CREATE TABLE IF NOT EXISTS products ("
    "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
    "  name TEXT NOT NULL,"
    "  price REAL,"
    "  stock INTEGER"
    ");";

char *err_msg = NULL;
int rc = sqlite3_exec_ptr(db, create_sql, NULL, NULL, &err_msg);

if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free_ptr(err_msg);
}

sqlite3_close_ptr(db);
```

### 2. INSERT/UPDATE/DELETE

```c
// Insert data
const char *insert = "INSERT INTO products (name, price, stock) VALUES ('Laptop', 999.99, 10);";
sqlite3_exec_ptr(db, insert, NULL, NULL, NULL);

// Update data
const char *update = "UPDATE products SET stock = stock - 1 WHERE name = 'Laptop';";
sqlite3_exec_ptr(db, update, NULL, NULL, NULL);

// Check affected rows
int changes = sqlite3_changes_ptr(db);
printf("Rows affected: %d\n", changes);

// Delete data
const char *delete_sql = "DELETE FROM products WHERE stock = 0;";
sqlite3_exec_ptr(db, delete_sql, NULL, NULL, NULL);
```

### 3. Prepared Statements

```c
sqlite3_stmt *stmt;
const char *sql = "SELECT id, name, price FROM products WHERE price < ?;";

// Prepare statement
int rc = sqlite3_prepare_v2_ptr(db, sql, -1, &stmt, NULL);

if (rc == SQLITE_OK) {
    // Bind parameters
    sqlite3_bind_double_ptr(stmt, 1, 100.0);  // price < 100

    // Execute and fetch results
    while (sqlite3_step_ptr(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int_ptr(stmt, 0);
        const unsigned char *name = sqlite3_column_text_ptr(stmt, 1);
        double price = sqlite3_column_double_ptr(stmt, 2);

        printf("ID: %d, Name: %s, Price: $%.2f\n", id, name, price);
    }

    sqlite3_finalize_ptr(stmt);
}
```

### 4. Transactions

```c
// Begin transaction
sqlite3_exec_ptr(db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

// Multiple operations
sqlite3_exec_ptr(db, "INSERT INTO products VALUES (1, 'Item1', 10.0, 5);", NULL, NULL, NULL);
sqlite3_exec_ptr(db, "INSERT INTO products VALUES (2, 'Item2', 20.0, 3);", NULL, NULL, NULL);
sqlite3_exec_ptr(db, "UPDATE products SET price = price * 1.1;", NULL, NULL, NULL);

// Commit or rollback
int rc = sqlite3_exec_ptr(db, "COMMIT;", NULL, NULL, NULL);
if (rc != SQLITE_OK) {
    sqlite3_exec_ptr(db, "ROLLBACK;", NULL, NULL, NULL);
    fprintf(stderr, "Transaction failed, rolled back\n");
}
```

## API Reference

### Database Operations

#### `int sqlite3_open(const char *filename, sqlite3 **ppDb)`
Open database connection.
- **filename**: Database file path or `:memory:` for in-memory DB
- **Returns**: SQLITE_OK (0) on success
- **Must close**: `sqlite3_close(db)`

#### `int sqlite3_close(sqlite3 *db)`
Close database connection.
- **Returns**: SQLITE_OK on success

#### `int sqlite3_exec(sqlite3 *db, const char *sql, callback, void *arg, char **errmsg)`
Execute SQL statement.
- **sql**: SQL command to execute
- **callback**: Optional callback for SELECT results
- **errmsg**: Error message output (must free with `sqlite3_free()`)

### Prepared Statements

#### `int sqlite3_prepare_v2(sqlite3 *db, const char *zSql, int nByte, sqlite3_stmt **ppStmt, const char **pzTail)`
Compile SQL into prepared statement.
- **nByte**: SQL length (-1 for null-terminated)
- **Returns**: SQLITE_OK on success
- **Must finalize**: `sqlite3_finalize(stmt)`

#### `int sqlite3_step(sqlite3_stmt *stmt)`
Execute prepared statement.
- **Returns**: SQLITE_ROW if data available, SQLITE_DONE when finished

#### `int sqlite3_reset(sqlite3_stmt *stmt)`
Reset prepared statement for re-execution.

#### `int sqlite3_finalize(sqlite3_stmt *stmt)`
Destroy prepared statement.

### Parameter Binding

#### `int sqlite3_bind_text(sqlite3_stmt *stmt, int index, const char *value, int n, void(*destructor)(void*))`
Bind text parameter.
- **index**: Parameter index (1-based)
- **n**: Length (-1 for null-terminated)

#### `int sqlite3_bind_int(sqlite3_stmt *stmt, int index, int value)`
Bind integer parameter.

#### `int sqlite3_bind_double(sqlite3_stmt *stmt, int index, double value)`
Bind double parameter.

#### `int sqlite3_bind_null(sqlite3_stmt *stmt, int index)`
Bind NULL parameter.

### Result Extraction

#### `int sqlite3_column_count(sqlite3_stmt *stmt)`
Get number of columns in result.

#### `const char* sqlite3_column_name(sqlite3_stmt *stmt, int N)`
Get column name.

#### `const unsigned char* sqlite3_column_text(sqlite3_stmt *stmt, int iCol)`
Get text column value.

#### `int sqlite3_column_int(sqlite3_stmt *stmt, int iCol)`
Get integer column value.

#### `double sqlite3_column_double(sqlite3_stmt *stmt, int iCol)`
Get double column value.

#### `int sqlite3_column_bytes(sqlite3_stmt *stmt, int iCol)`
Get size of column data in bytes.

### Utility Functions

#### `const char* sqlite3_errmsg(sqlite3 *db)`
Get last error message.

#### `int sqlite3_changes(sqlite3 *db)`
Get number of rows modified by last statement.

#### `long long sqlite3_last_insert_rowid(sqlite3 *db)`
Get rowid of last inserted row.

#### `const char* sqlite3_libversion(void)`
Get SQLite library version string.

## Error Handling

### Check Return Codes

```c
int rc = sqlite3_open_ptr("mydb.db", &db);
if (rc != SQLITE_OK) {
    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg_ptr(db));
    return -1;
}
```

### Handle Execution Errors

```c
char *err_msg = NULL;
int rc = sqlite3_exec_ptr(db, sql, NULL, NULL, &err_msg);

if (rc != SQLITE_OK) {
    fprintf(stderr, "SQL error: %s\n", err_msg);
    sqlite3_free_ptr(err_msg);
    return -1;
}
```

### Prepared Statement Errors

```c
sqlite3_stmt *stmt;
int rc = sqlite3_prepare_v2_ptr(db, sql, -1, &stmt, NULL);

if (rc != SQLITE_OK) {
    fprintf(stderr, "Prepare failed: %s\n", sqlite3_errmsg_ptr(db));
    return -1;
}
```

## SQLite Return Codes

```c
#define SQLITE_OK           0   /* Success */
#define SQLITE_ERROR        1   /* SQL error */
#define SQLITE_BUSY         5   /* Database is locked */
#define SQLITE_NOMEM        7   /* malloc() failed */
#define SQLITE_READONLY     8   /* Attempt to write readonly DB */
#define SQLITE_CONSTRAINT  19   /* Constraint violation */
#define SQLITE_ROW        100   /* sqlite3_step() has another row */
#define SQLITE_DONE       101   /* sqlite3_step() finished */
```

## Performance Tips

1. **Use transactions**: Wrap bulk INSERT/UPDATE in transactions for 10-100x speedup

2. **Prepared statements**: Reuse prepared statements instead of re-parsing SQL

3. **Indexes**: Create indexes on frequently queried columns
   ```sql
   CREATE INDEX idx_name ON products(name);
   ```

4. **PRAGMA settings**: Optimize for your use case
   ```sql
   PRAGMA synchronous = NORMAL;
   PRAGMA cache_size = 10000;
   PRAGMA temp_store = MEMORY;
   ```

5. **Batch operations**: Group multiple operations in single transaction

## Common Pitfalls

- **File locks**: SQLite locks entire database file (consider WAL mode for concurrency)
- **Memory leaks**: Always `sqlite3_finalize()` prepared statements
- **Error messages**: Free error messages with `sqlite3_free()`
- **Thread safety**: SQLite is thread-safe but connections should not be shared across threads
- **Parameter binding**: Indices are 1-based, not 0-based
- **Column types**: SQLite uses dynamic typing (values have types, not columns)

## Database Files

### In-Memory Database
```c
sqlite3_open_ptr(":memory:", &db);  // Fastest, lost on close
```

### Persistent Database
```c
sqlite3_open_ptr("myapp.db", &db);  // Saved to disk
```

### Temporary Database
```c
sqlite3_open_ptr("", &db);  // Disk-based but deleted on close
```

## Library Detection

The module attempts to load SQLite3 from:

**Linux**:
- `lib/libsqlite3.so`
- `/usr/lib/libsqlite3.so`
- `/usr/local/lib/libsqlite3.so`

**macOS**:
- `lib/libsqlite3.dylib`
- `/usr/lib/libsqlite3.dylib`
- `/usr/local/lib/libsqlite3.dylib`

**Windows**:
- `lib/sqlite3.dll`
- `sqlite3.dll`

## Installation

### Ubuntu/Debian
```bash
sudo apt-get install libsqlite3-dev
```

### macOS
```bash
brew install sqlite3
```

### Windows
Download from: https://www.sqlite.org/download.html

## Example Output

```
========================================
  Basic SQLite Operations
========================================

1. Open In-Memory Database:
   Database opened successfully

2. Create Table:
   Table 'users' created

3. Insert Data:
   Inserted 3 rows

4. Query Data:
   Users with age >= 30:
     ID=1, Name=Alice, Email=alice@example.com, Age=30
     ID=3, Name=Charlie, Email=charlie@example.com, Age=35

✓ All SQLite examples completed!
```

## Related Modules

- **mod_json**: Store JSON in SQLite using JSON1 extension
- **mod_http**: Build REST APIs backed by SQLite

## See Also

- Full example: `c_modules/examples/example_sqlite.c`
- SQLite documentation: https://www.sqlite.org/docs.html
- SQL tutorial: https://www.sqlite.org/lang.html
