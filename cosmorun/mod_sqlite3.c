/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ SQLite3 Dynamic Library Wrapper for cosmorun                               │
│ Loads SQLite3 via dlopen() - architecture-specific .dylib/.so/.dll         │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "cosmo_libc.h"

// SQLite3 API function pointers
static void *sqlite3_lib_handle = NULL;
static char last_error_msg[1024] = {0};

// SQLite3 types (opaque)
typedef struct sqlite3 sqlite3;
typedef struct sqlite3_stmt sqlite3_stmt;

// SQLite3 API function pointer types
typedef int (*sqlite3_open_t)(const char *filename, sqlite3 **ppDb);
typedef int (*sqlite3_close_t)(sqlite3 *db);
typedef int (*sqlite3_exec_t)(sqlite3 *db, const char *sql, 
                              int (*callback)(void*,int,char**,char**), 
                              void *arg, char **errmsg);
typedef int (*sqlite3_prepare_v2_t)(sqlite3 *db, const char *zSql, int nByte,
                                    sqlite3_stmt **ppStmt, const char **pzTail);
typedef int (*sqlite3_step_t)(sqlite3_stmt *pStmt);
typedef int (*sqlite3_finalize_t)(sqlite3_stmt *pStmt);
typedef int (*sqlite3_bind_text_t)(sqlite3_stmt *pStmt, int idx, const char *val,
                                  int n, void(*destructor)(void*));
typedef int (*sqlite3_bind_int_t)(sqlite3_stmt *pStmt, int idx, int val);
typedef int (*sqlite3_bind_double_t)(sqlite3_stmt *pStmt, int idx, double val);
typedef const unsigned char *(*sqlite3_column_text_t)(sqlite3_stmt *pStmt, int iCol);
typedef int (*sqlite3_column_int_t)(sqlite3_stmt *pStmt, int iCol);
typedef double (*sqlite3_column_double_t)(sqlite3_stmt *pStmt, int iCol);
typedef int (*sqlite3_column_count_t)(sqlite3_stmt *pStmt);
typedef const char *(*sqlite3_column_name_t)(sqlite3_stmt *pStmt, int N);
typedef const char *(*sqlite3_errmsg_t)(sqlite3 *db);
typedef const char *(*sqlite3_libversion_t)(void);
typedef int (*sqlite3_changes_t)(sqlite3 *db);
typedef long long (*sqlite3_last_insert_rowid_t)(sqlite3 *db);

// Function pointers
static sqlite3_open_t sqlite3_open_ptr = NULL;
static sqlite3_close_t sqlite3_close_ptr = NULL;
static sqlite3_exec_t sqlite3_exec_ptr = NULL;
static sqlite3_prepare_v2_t sqlite3_prepare_v2_ptr = NULL;
static sqlite3_step_t sqlite3_step_ptr = NULL;
static sqlite3_finalize_t sqlite3_finalize_ptr = NULL;
static sqlite3_bind_text_t sqlite3_bind_text_ptr = NULL;
static sqlite3_bind_int_t sqlite3_bind_int_ptr = NULL;
static sqlite3_bind_double_t sqlite3_bind_double_ptr = NULL;
static sqlite3_column_text_t sqlite3_column_text_ptr = NULL;
static sqlite3_column_int_t sqlite3_column_int_ptr = NULL;
static sqlite3_column_double_t sqlite3_column_double_ptr = NULL;
static sqlite3_column_count_t sqlite3_column_count_ptr = NULL;
static sqlite3_column_name_t sqlite3_column_name_ptr = NULL;
static sqlite3_errmsg_t sqlite3_errmsg_ptr = NULL;
static sqlite3_libversion_t sqlite3_libversion_ptr = NULL;
static sqlite3_changes_t sqlite3_changes_ptr = NULL;
static sqlite3_last_insert_rowid_t sqlite3_last_insert_rowid_ptr = NULL;

// SQLite3 constants
#define SQLITE_OK           0   /* Successful result */
#define SQLITE_ERROR        1   /* SQL error or missing database */
#define SQLITE_BUSY         5   /* The database file is locked */
#define SQLITE_LOCKED       6   /* A table in the database is locked */
#define SQLITE_NOMEM        7   /* A malloc() failed */
#define SQLITE_READONLY     8   /* Attempt to write a readonly database */
#define SQLITE_INTERRUPT    9   /* Operation terminated by sqlite3_interrupt()*/
#define SQLITE_IOERR       10   /* Some kind of disk I/O error occurred */
#define SQLITE_CORRUPT     11   /* The database disk image is malformed */
#define SQLITE_NOTFOUND    12   /* Unknown opcode in sqlite3_file_control() */
#define SQLITE_FULL        13   /* Insertion failed because database is full */
#define SQLITE_CANTOPEN    14   /* Unable to open the database file */
#define SQLITE_PROTOCOL    15   /* Database lock protocol error */
#define SQLITE_EMPTY       16   /* Database is empty */
#define SQLITE_SCHEMA      17   /* The database schema changed */
#define SQLITE_TOOBIG      18   /* String or BLOB exceeds size limit */
#define SQLITE_CONSTRAINT  19   /* Abort due to constraint violation */
#define SQLITE_MISMATCH    20   /* Data type mismatch */
#define SQLITE_MISUSE      21   /* Library used incorrectly */
#define SQLITE_NOLFS       22   /* Uses OS features not supported on host */
#define SQLITE_AUTH        23   /* Authorization denied */
#define SQLITE_FORMAT      24   /* Auxiliary database format error */
#define SQLITE_RANGE       25   /* 2nd parameter to sqlite3_bind out of range */
#define SQLITE_NOTADB      26   /* File opened that is not a database file */
#define SQLITE_NOTICE      27   /* Notifications from sqlite3_log() */
#define SQLITE_WARNING     28   /* Warnings from sqlite3_log() */
#define SQLITE_ROW         100  /* sqlite3_step() has another row ready */
#define SQLITE_DONE        101  /* sqlite3_step() has finished executing */

#define SQLITE_STATIC      ((void(*)(void *))0)
#define SQLITE_TRANSIENT   ((void(*)(void *))-1)

// Platform detection and library loading
static const char* get_sqlite3_library_name(void) {
  // Simple runtime detection by trying to load libraries
  const char* candidates[] = {
    "lib/sqlite3-arm-64.dylib",   // macOS ARM64
    "lib/sqlite3-x86-64.dylib",  // macOS x86_64
    "lib/sqlite3-arm-64.so",     // Linux ARM64
    "lib/sqlite3-x86-64.so",     // Linux x86_64
    "lib/sqlite3-arm-64.dll",    // Windows ARM64
    "lib/sqlite3-x86-64.dll",    // Windows x86_64
    NULL
  };

  // Try each candidate and return the first one that loads successfully
  for (int i = 0; candidates[i]; i++) {
    void* test_handle = cosmo_dlopen(candidates[i], RTLD_LAZY);
    if (test_handle) {
      cosmo_dlclose(test_handle);
      printf("🔍 Selected SQLite3 library: %s\n", candidates[i]);
      return candidates[i];
    } else {
      printf("🔍 Failed to load %s: %s\n", candidates[i], cosmo_dlerror());
    }
  }

  // Fallback - this will likely fail but provides a clear error message
  return "lib/sqlite3-x86-64.so";
}

static int load_sqlite3_symbols(void) {
  if (sqlite3_lib_handle) return 1; // Already loaded
  
  const char* lib_name = get_sqlite3_library_name();
  
  // Try to load the library
  sqlite3_lib_handle = cosmo_dlopen(lib_name, RTLD_LAZY);
  if (!sqlite3_lib_handle) {
    snprintf(last_error_msg, sizeof(last_error_msg),
             "Failed to load SQLite3 library '%s': %s", lib_name, cosmo_dlerror());
    return 0;
  }
  
  // Load function symbols
  #define LOAD_SYMBOL(name) do { \
    name##_ptr = (name##_t)cosmo_dlsym(sqlite3_lib_handle, #name); \
    if (!name##_ptr) { \
      snprintf(last_error_msg, sizeof(last_error_msg), \
               "Failed to load symbol '%s': %s", #name, cosmo_dlerror()); \
      cosmo_dlclose(sqlite3_lib_handle); \
      sqlite3_lib_handle = NULL; \
      return 0; \
    } \
  } while(0)
  
  LOAD_SYMBOL(sqlite3_open);
  LOAD_SYMBOL(sqlite3_close);
  LOAD_SYMBOL(sqlite3_exec);
  LOAD_SYMBOL(sqlite3_prepare_v2);
  LOAD_SYMBOL(sqlite3_step);
  LOAD_SYMBOL(sqlite3_finalize);
  LOAD_SYMBOL(sqlite3_bind_text);
  LOAD_SYMBOL(sqlite3_bind_int);
  LOAD_SYMBOL(sqlite3_bind_double);
  LOAD_SYMBOL(sqlite3_column_text);
  LOAD_SYMBOL(sqlite3_column_int);
  LOAD_SYMBOL(sqlite3_column_double);
  LOAD_SYMBOL(sqlite3_column_count);
  LOAD_SYMBOL(sqlite3_column_name);
  LOAD_SYMBOL(sqlite3_errmsg);
  LOAD_SYMBOL(sqlite3_libversion);
  LOAD_SYMBOL(sqlite3_changes);
  LOAD_SYMBOL(sqlite3_last_insert_rowid);
  
  #undef LOAD_SYMBOL
  
  return 1;
}

// Public API functions
const char* sqlite3_get_last_error(void) {
  return last_error_msg;
}

const char* sqlite3_get_version(void) {
  if (!load_sqlite3_symbols()) return NULL;
  return sqlite3_libversion_ptr();
}

int sqlite3_init(void) {
  return load_sqlite3_symbols();
}

void sqlite3_cleanup(void) {
  if (sqlite3_lib_handle) {
    cosmo_dlclose(sqlite3_lib_handle);
    sqlite3_lib_handle = NULL;
  }
}

// Wrapper functions for SQLite3 API
int sqlite3_open_wrapper(const char *filename, sqlite3 **ppDb) {
  if (!load_sqlite3_symbols()) return SQLITE_ERROR;
  return sqlite3_open_ptr(filename, ppDb);
}

int sqlite3_close_wrapper(sqlite3 *db) {
  if (!sqlite3_close_ptr) return SQLITE_ERROR;
  return sqlite3_close_ptr(db);
}

int sqlite3_exec_wrapper(sqlite3 *db, const char *sql, 
                        int (*callback)(void*,int,char**,char**), 
                        void *arg, char **errmsg) {
  if (!sqlite3_exec_ptr) return SQLITE_ERROR;
  return sqlite3_exec_ptr(db, sql, callback, arg, errmsg);
}

int sqlite3_prepare_v2_wrapper(sqlite3 *db, const char *zSql, int nByte,
                               sqlite3_stmt **ppStmt, const char **pzTail) {
  if (!sqlite3_prepare_v2_ptr) return SQLITE_ERROR;
  return sqlite3_prepare_v2_ptr(db, zSql, nByte, ppStmt, pzTail);
}

int sqlite3_step_wrapper(sqlite3_stmt *pStmt) {
  if (!sqlite3_step_ptr) return SQLITE_ERROR;
  return sqlite3_step_ptr(pStmt);
}

int sqlite3_finalize_wrapper(sqlite3_stmt *pStmt) {
  if (!sqlite3_finalize_ptr) return SQLITE_ERROR;
  return sqlite3_finalize_ptr(pStmt);
}

int sqlite3_bind_text_wrapper(sqlite3_stmt *pStmt, int idx, const char *val,
                              int n, void(*destructor)(void*)) {
  if (!sqlite3_bind_text_ptr) return SQLITE_ERROR;
  return sqlite3_bind_text_ptr(pStmt, idx, val, n, destructor);
}

int sqlite3_bind_int_wrapper(sqlite3_stmt *pStmt, int idx, int val) {
  if (!sqlite3_bind_int_ptr) return SQLITE_ERROR;
  return sqlite3_bind_int_ptr(pStmt, idx, val);
}

int sqlite3_bind_double_wrapper(sqlite3_stmt *pStmt, int idx, double val) {
  if (!sqlite3_bind_double_ptr) return SQLITE_ERROR;
  return sqlite3_bind_double_ptr(pStmt, idx, val);
}

const unsigned char *sqlite3_column_text_wrapper(sqlite3_stmt *pStmt, int iCol) {
  if (!sqlite3_column_text_ptr) return NULL;
  return sqlite3_column_text_ptr(pStmt, iCol);
}

int sqlite3_column_int_wrapper(sqlite3_stmt *pStmt, int iCol) {
  if (!sqlite3_column_int_ptr) return 0;
  return sqlite3_column_int_ptr(pStmt, iCol);
}

double sqlite3_column_double_wrapper(sqlite3_stmt *pStmt, int iCol) {
  if (!sqlite3_column_double_ptr) return 0.0;
  return sqlite3_column_double_ptr(pStmt, iCol);
}

int sqlite3_column_count_wrapper(sqlite3_stmt *pStmt) {
  if (!sqlite3_column_count_ptr) return 0;
  return sqlite3_column_count_ptr(pStmt);
}

const char *sqlite3_column_name_wrapper(sqlite3_stmt *pStmt, int N) {
  if (!sqlite3_column_name_ptr) return NULL;
  return sqlite3_column_name_ptr(pStmt, N);
}

const char *sqlite3_errmsg_wrapper(sqlite3 *db) {
  if (!sqlite3_errmsg_ptr) return "SQLite3 not loaded";
  return sqlite3_errmsg_ptr(db);
}

int sqlite3_changes_wrapper(sqlite3 *db) {
  if (!sqlite3_changes_ptr) return 0;
  return sqlite3_changes_ptr(db);
}

long long sqlite3_last_insert_rowid_wrapper(sqlite3 *db) {
  if (!sqlite3_last_insert_rowid_ptr) return 0;
  return sqlite3_last_insert_rowid_ptr(db);
}
