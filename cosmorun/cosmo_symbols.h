// Symbol indexing and navigation for LSP
// Provides symbol extraction, indexing, and query capabilities

#ifndef COSMO_SYMBOLS_H
#define COSMO_SYMBOLS_H

#include <stddef.h>

// Symbol kinds
typedef enum {
    SYMBOL_FUNCTION,
    SYMBOL_VARIABLE,
    SYMBOL_TYPEDEF,
    SYMBOL_STRUCT,
    SYMBOL_ENUM,
    SYMBOL_MACRO,
    SYMBOL_UNKNOWN
} symbol_kind_t;

// Symbol information
typedef struct {
    char *name;              // Symbol name
    symbol_kind_t kind;      // Symbol kind
    char *file;              // File path
    int line;                // Line number (1-based)
    int column;              // Column number (0-based)
    char *signature;         // Full signature (e.g., "int foo(int x)")
    char *scope;             // Scope (e.g., "global", "struct Point")
} symbol_info_t;

// Symbol location (for references)
typedef struct {
    char *file;              // File path
    int line;                // Line number (1-based)
    int column;              // Column number (0-based)
} symbol_location_t;

// Symbol reference (symbol + locations)
typedef struct {
    symbol_info_t *symbol;   // Symbol info
    symbol_location_t *locations; // Reference locations
    int location_count;      // Number of locations
} symbol_reference_t;

// Opaque symbol index type
typedef struct symbol_index_t symbol_index_t;

// Symbol index management
symbol_index_t* symbol_index_create(void);
void symbol_index_destroy(symbol_index_t *idx);

// Symbol operations
int symbol_index_add(symbol_index_t *idx, symbol_info_t *symbol);
int symbol_index_parse_file(symbol_index_t *idx, const char *file, const char *source);
int symbol_index_remove_file(symbol_index_t *idx, const char *file);

// Symbol queries
symbol_info_t* symbol_index_find_definition(symbol_index_t *idx, const char *name);
symbol_info_t* symbol_index_find_at_position(symbol_index_t *idx, const char *file, int line, int column);
symbol_location_t* symbol_index_find_references(symbol_index_t *idx, const char *name, int *count);
symbol_info_t* symbol_index_search(symbol_index_t *idx, const char *query, int *count);
symbol_info_t* symbol_index_list_file_symbols(symbol_index_t *idx, const char *file, int *count);

// Symbol info helpers
symbol_info_t* symbol_info_create(const char *name, symbol_kind_t kind, const char *file, int line, int column);
void symbol_info_destroy(symbol_info_t *symbol);
symbol_info_t* symbol_info_clone(const symbol_info_t *symbol);

// Symbol location helpers
symbol_location_t* symbol_location_create(const char *file, int line, int column);
void symbol_location_destroy(symbol_location_t *location);

// Symbol extraction from source
int extract_symbols_from_source(const char *source, const char *file, symbol_info_t **symbols, int *count);
symbol_info_t* find_symbol_at_position(const char *source, const char *file, int line, int column);

// Symbol kind utilities
const char* symbol_kind_to_string(symbol_kind_t kind);
symbol_kind_t symbol_kind_from_string(const char *kind_str);

// Debug utilities
void symbol_index_dump(symbol_index_t *idx);
void symbol_info_print(const symbol_info_t *symbol);

#endif // COSMO_SYMBOLS_H
