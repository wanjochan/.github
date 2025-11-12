// Symbol indexing and navigation implementation
#include "cosmo_symbols.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Hash table constants
#define SYMBOL_TABLE_SIZE 1024
#define MAX_LINE_LENGTH 1024

// Hash table entry
typedef struct symbol_entry_t {
    symbol_info_t *symbol;
    struct symbol_entry_t *next;
} symbol_entry_t;

// Symbol index structure
struct symbol_index_t {
    symbol_entry_t *table[SYMBOL_TABLE_SIZE];
    int count;
};

// Hash function
static unsigned int hash_string(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % SYMBOL_TABLE_SIZE;
}

// String helpers
static char* str_duplicate(const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char *dup = (char*)malloc(len + 1);
    if (dup) {
        memcpy(dup, str, len + 1);
    }
    return dup;
}

static void str_free(char *str) {
    free(str);
}

// Symbol info operations
symbol_info_t* symbol_info_create(const char *name, symbol_kind_t kind, const char *file, int line, int column) {
    symbol_info_t *symbol = (symbol_info_t*)malloc(sizeof(symbol_info_t));
    if (!symbol) return NULL;

    symbol->name = str_duplicate(name);
    symbol->kind = kind;
    symbol->file = str_duplicate(file);
    symbol->line = line;
    symbol->column = column;
    symbol->signature = NULL;
    symbol->scope = str_duplicate("global");

    return symbol;
}

void symbol_info_destroy(symbol_info_t *symbol) {
    if (!symbol) return;
    str_free(symbol->name);
    str_free(symbol->file);
    str_free(symbol->signature);
    str_free(symbol->scope);
    free(symbol);
}

symbol_info_t* symbol_info_clone(const symbol_info_t *symbol) {
    if (!symbol) return NULL;

    symbol_info_t *clone = symbol_info_create(symbol->name, symbol->kind, symbol->file, symbol->line, symbol->column);
    if (!clone) return NULL;

    if (symbol->signature) {
        str_free(clone->signature);
        clone->signature = str_duplicate(symbol->signature);
    }
    if (symbol->scope) {
        str_free(clone->scope);
        clone->scope = str_duplicate(symbol->scope);
    }

    return clone;
}

// Symbol location operations
symbol_location_t* symbol_location_create(const char *file, int line, int column) {
    symbol_location_t *loc = (symbol_location_t*)malloc(sizeof(symbol_location_t));
    if (!loc) return NULL;

    loc->file = str_duplicate(file);
    loc->line = line;
    loc->column = column;

    return loc;
}

void symbol_location_destroy(symbol_location_t *location) {
    if (!location) return;
    str_free(location->file);
    free(location);
}

// Symbol index operations
symbol_index_t* symbol_index_create(void) {
    symbol_index_t *idx = (symbol_index_t*)malloc(sizeof(symbol_index_t));
    if (!idx) return NULL;

    memset(idx->table, 0, sizeof(idx->table));
    idx->count = 0;

    return idx;
}

void symbol_index_destroy(symbol_index_t *idx) {
    if (!idx) return;

    for (int i = 0; i < SYMBOL_TABLE_SIZE; i++) {
        symbol_entry_t *entry = idx->table[i];
        while (entry) {
            symbol_entry_t *next = entry->next;
            symbol_info_destroy(entry->symbol);
            free(entry);
            entry = next;
        }
    }

    free(idx);
}

int symbol_index_add(symbol_index_t *idx, symbol_info_t *symbol) {
    if (!idx || !symbol || !symbol->name) return -1;

    unsigned int hash = hash_string(symbol->name);

    symbol_entry_t *entry = (symbol_entry_t*)malloc(sizeof(symbol_entry_t));
    if (!entry) return -1;

    entry->symbol = symbol;
    entry->next = idx->table[hash];
    idx->table[hash] = entry;
    idx->count++;

    return 0;
}

symbol_info_t* symbol_index_find_definition(symbol_index_t *idx, const char *name) {
    if (!idx || !name) return NULL;

    unsigned int hash = hash_string(name);
    symbol_entry_t *entry = idx->table[hash];

    while (entry) {
        if (entry->symbol && entry->symbol->name && strcmp(entry->symbol->name, name) == 0) {
            return entry->symbol;
        }
        entry = entry->next;
    }

    return NULL;
}

symbol_info_t* symbol_index_find_at_position(symbol_index_t *idx, const char *file, int line, int column) {
    if (!idx || !file) return NULL;

    for (int i = 0; i < SYMBOL_TABLE_SIZE; i++) {
        symbol_entry_t *entry = idx->table[i];
        while (entry) {
            symbol_info_t *sym = entry->symbol;
            if (sym && sym->file && strcmp(sym->file, file) == 0 && sym->line == line) {
                // Check if column is within symbol name range
                if (column >= sym->column && column < sym->column + (int)strlen(sym->name)) {
                    return sym;
                }
            }
            entry = entry->next;
        }
    }

    return NULL;
}

symbol_location_t* symbol_index_find_references(symbol_index_t *idx, const char *name, int *count) {
    if (!idx || !name || !count) return NULL;

    *count = 0;

    // Find definition first
    symbol_info_t *def = symbol_index_find_definition(idx, name);
    if (!def) return NULL;

    // For now, return just the definition location
    symbol_location_t *locations = (symbol_location_t*)malloc(sizeof(symbol_location_t));
    if (!locations) return NULL;

    locations[0].file = str_duplicate(def->file);
    locations[0].line = def->line;
    locations[0].column = def->column;
    *count = 1;

    return locations;
}

symbol_info_t* symbol_index_search(symbol_index_t *idx, const char *query, int *count) {
    if (!idx || !query || !count) return NULL;

    *count = 0;
    int capacity = 10;
    symbol_info_t *results = (symbol_info_t*)malloc(capacity * sizeof(symbol_info_t));
    if (!results) return NULL;

    for (int i = 0; i < SYMBOL_TABLE_SIZE; i++) {
        symbol_entry_t *entry = idx->table[i];
        while (entry) {
            if (entry->symbol && entry->symbol->name && strstr(entry->symbol->name, query)) {
                if (*count >= capacity) {
                    capacity *= 2;
                    symbol_info_t *new_results = (symbol_info_t*)realloc(results, capacity * sizeof(symbol_info_t));
                    if (!new_results) {
                        free(results);
                        return NULL;
                    }
                    results = new_results;
                }

                results[*count] = *symbol_info_clone(entry->symbol);
                (*count)++;
            }
            entry = entry->next;
        }
    }

    return results;
}

symbol_info_t* symbol_index_list_file_symbols(symbol_index_t *idx, const char *file, int *count) {
    if (!idx || !file || !count) return NULL;

    *count = 0;
    int capacity = 20;
    symbol_info_t *results = (symbol_info_t*)malloc(capacity * sizeof(symbol_info_t));
    if (!results) return NULL;

    for (int i = 0; i < SYMBOL_TABLE_SIZE; i++) {
        symbol_entry_t *entry = idx->table[i];
        while (entry) {
            if (entry->symbol && entry->symbol->file && strcmp(entry->symbol->file, file) == 0) {
                if (*count >= capacity) {
                    capacity *= 2;
                    symbol_info_t *new_results = (symbol_info_t*)realloc(results, capacity * sizeof(symbol_info_t));
                    if (!new_results) {
                        free(results);
                        return NULL;
                    }
                    results = new_results;
                }

                results[*count] = *symbol_info_clone(entry->symbol);
                (*count)++;
            }
            entry = entry->next;
        }
    }

    return results;
}

int symbol_index_remove_file(symbol_index_t *idx, const char *file) {
    if (!idx || !file) return -1;

    int removed = 0;
    for (int i = 0; i < SYMBOL_TABLE_SIZE; i++) {
        symbol_entry_t **entry_ptr = &idx->table[i];
        while (*entry_ptr) {
            symbol_entry_t *entry = *entry_ptr;
            if (entry->symbol && entry->symbol->file && strcmp(entry->symbol->file, file) == 0) {
                *entry_ptr = entry->next;
                symbol_info_destroy(entry->symbol);
                free(entry);
                removed++;
                idx->count--;
            } else {
                entry_ptr = &entry->next;
            }
        }
    }

    return removed;
}

// Symbol extraction helpers
static int is_identifier_char(char c) {
    return isalnum(c) || c == '_';
}

static int extract_identifier(const char *str, char *out, int max_len) {
    int i = 0;
    while (str[i] && is_identifier_char(str[i]) && i < max_len - 1) {
        out[i] = str[i];
        i++;
    }
    out[i] = '\0';
    return i;
}

static void skip_whitespace(const char **ptr) {
    while (**ptr && isspace(**ptr)) {
        (*ptr)++;
    }
}

static int extract_function_signature(const char *line, char *name, char *signature, int max_len) {
    // Pattern: type name(args)
    const char *ptr = line;
    skip_whitespace(&ptr);

    // Skip return type
    while (*ptr && !isspace(*ptr) && *ptr != '*') ptr++;
    while (*ptr && (*ptr == '*' || isspace(*ptr))) ptr++;

    // Extract function name
    int name_len = extract_identifier(ptr, name, max_len);
    if (name_len == 0) return 0;

    ptr += name_len;
    skip_whitespace(&ptr);

    // Check for opening parenthesis
    if (*ptr != '(') return 0;

    // Extract full signature
    const char *sig_start = line;
    while (*sig_start && isspace(*sig_start)) sig_start++;

    const char *sig_end = ptr;
    int depth = 1;
    sig_end++; // Skip '('

    while (*sig_end && depth > 0) {
        if (*sig_end == '(') depth++;
        else if (*sig_end == ')') depth--;
        sig_end++;
    }

    int sig_len = sig_end - sig_start;
    if (sig_len >= max_len) sig_len = max_len - 1;
    memcpy(signature, sig_start, sig_len);
    signature[sig_len] = '\0';

    return 1;
}

static int extract_variable_declaration(const char *line, char *name, int max_len) {
    // Pattern: type name; or type name = value;
    const char *ptr = line;
    skip_whitespace(&ptr);

    // Skip type keywords
    while (*ptr && !isspace(*ptr) && *ptr != '*') ptr++;
    while (*ptr && (*ptr == '*' || isspace(*ptr))) ptr++;

    // Extract variable name
    int name_len = extract_identifier(ptr, name, max_len);
    if (name_len == 0) return 0;

    ptr += name_len;
    skip_whitespace(&ptr);

    // Check for semicolon or assignment
    if (*ptr == ';' || *ptr == '=' || *ptr == ',') {
        return 1;
    }

    return 0;
}

static int extract_struct_name(const char *line, char *name, int max_len) {
    // Pattern: struct name { or struct name;
    const char *ptr = strstr(line, "struct");
    if (!ptr) return 0;

    ptr += 6; // Skip "struct"
    skip_whitespace(&ptr);

    int name_len = extract_identifier(ptr, name, max_len);
    if (name_len == 0) return 0;

    ptr += name_len;
    skip_whitespace(&ptr);

    // Check for valid struct declaration
    if (*ptr == '{' || *ptr == ';') {
        return 1;
    }

    return 0;
}

static int extract_typedef_name(const char *line, char *name, int max_len) {
    // Pattern: typedef ... name; or typedef ... (*name)(...);
    const char *ptr = strstr(line, "typedef");
    if (!ptr) return 0;

    ptr += 7; // Skip "typedef"

    // Find semicolon
    const char *semicolon = strchr(ptr, ';');
    if (!semicolon) return 0;

    // For function pointer typedefs like "typedef void (*callback_t)(int);"
    // we need to find the identifier inside (*identifier)
    const char *close_paren = semicolon - 1;
    while (close_paren > ptr && isspace(*close_paren)) close_paren--;

    if (*close_paren == ')') {
        // Likely a function pointer, look for (*identifier) pattern
        const char *search = ptr;
        while (search < semicolon) {
            if (*search == '(' && *(search + 1) == '*') {
                // Found (*...), extract identifier
                const char *id_start = search + 2;
                while (*id_start && isspace(*id_start)) id_start++;
                const char *id_end = id_start;
                while (*id_end && is_identifier_char(*id_end)) id_end++;

                int name_len = id_end - id_start;
                if (name_len > 0 && name_len < max_len) {
                    memcpy(name, id_start, name_len);
                    name[name_len] = '\0';
                    return 1;
                }
            }
            search++;
        }
    }

    // Simple typedef: typedef type name;
    // Backtrack from semicolon to find last identifier
    const char *end = semicolon - 1;
    while (end > ptr && isspace(*end)) end--;
    end++;

    const char *start = end - 1;
    while (start > ptr && is_identifier_char(*start)) start--;
    if (!is_identifier_char(*start)) start++;

    int name_len = end - start;
    if (name_len >= max_len) name_len = max_len - 1;
    if (name_len > 0) {
        memcpy(name, start, name_len);
        name[name_len] = '\0';
        return 1;
    }

    return 0;
}

int extract_symbols_from_source(const char *source, const char *file, symbol_info_t **symbols, int *count) {
    if (!source || !file || !symbols || !count) return -1;

    *count = 0;
    int capacity = 50;
    *symbols = (symbol_info_t*)malloc(capacity * sizeof(symbol_info_t));
    if (!*symbols) return -1;

    char line[MAX_LINE_LENGTH];
    const char *ptr = source;
    int line_num = 1;

    while (*ptr) {
        // Extract line
        int i = 0;
        while (*ptr && *ptr != '\n' && i < MAX_LINE_LENGTH - 1) {
            line[i++] = *ptr++;
        }
        line[i] = '\0';
        if (*ptr == '\n') ptr++;

        // Trim line
        char *trimmed = line;
        while (*trimmed && isspace(*trimmed)) trimmed++;

        // Skip empty lines and comments
        if (*trimmed == '\0' || strncmp(trimmed, "//", 2) == 0) {
            line_num++;
            continue;
        }

        // Try to extract symbols
        char name[256] = {0};
        char signature[512] = {0};
        symbol_kind_t kind = SYMBOL_UNKNOWN;
        int found = 0;

        // Check for typedef (must be first, as it may contain struct/other keywords)
        if (strstr(trimmed, "typedef")) {
            if (extract_typedef_name(trimmed, name, 256)) {
                kind = SYMBOL_TYPEDEF;
                found = 1;
            }
        }
        // Check for function
        else if (strchr(trimmed, '(') && !strstr(trimmed, "typedef")) {
            if (extract_function_signature(trimmed, name, signature, 256)) {
                kind = SYMBOL_FUNCTION;
                found = 1;
            }
        }
        // Check for struct
        else if (strstr(trimmed, "struct")) {
            if (extract_struct_name(trimmed, name, 256)) {
                kind = SYMBOL_STRUCT;
                found = 1;
            }
        }
        // Check for variable
        else if (strchr(trimmed, ';')) {
            if (extract_variable_declaration(trimmed, name, 256)) {
                kind = SYMBOL_VARIABLE;
                found = 1;
            }
        }

        if (found && name[0]) {
            if (*count >= capacity) {
                capacity *= 2;
                symbol_info_t *new_symbols = (symbol_info_t*)realloc(*symbols, capacity * sizeof(symbol_info_t));
                if (!new_symbols) {
                    free(*symbols);
                    return -1;
                }
                *symbols = new_symbols;
            }

            // Find actual column of symbol name in trimmed line
            const char *name_pos = strstr(trimmed, name);
            int column = 0;
            if (name_pos) {
                column = (trimmed - line) + (name_pos - trimmed);
            }

            symbol_info_t *sym = symbol_info_create(name, kind, file, line_num, column);
            if (sym) {
                if (signature[0]) {
                    sym->signature = str_duplicate(signature);
                }
                (*symbols)[*count] = *sym;
                free(sym);
                (*count)++;
            }
        }

        line_num++;
    }

    return 0;
}

symbol_info_t* find_symbol_at_position(const char *source, const char *file, int line, int column) {
    if (!source || !file) return NULL;

    const char *ptr = source;
    int current_line = 1;

    // Navigate to target line
    while (*ptr && current_line < line) {
        if (*ptr == '\n') current_line++;
        ptr++;
    }

    if (current_line != line) return NULL;

    // Extract identifier at column
    const char *line_start = ptr;
    ptr += column;

    if (!*ptr || !is_identifier_char(*ptr)) return NULL;

    // Find identifier boundaries
    const char *id_start = ptr;
    while (id_start > line_start && is_identifier_char(*(id_start - 1))) {
        id_start--;
    }

    const char *id_end = ptr;
    while (*id_end && is_identifier_char(*id_end)) {
        id_end++;
    }

    // Extract identifier
    int len = id_end - id_start;
    if (len == 0 || len > 255) return NULL;

    char name[256];
    memcpy(name, id_start, len);
    name[len] = '\0';

    return symbol_info_create(name, SYMBOL_UNKNOWN, file, line, id_start - line_start);
}

int symbol_index_parse_file(symbol_index_t *idx, const char *file, const char *source) {
    if (!idx || !file || !source) return -1;

    // Remove existing symbols from this file
    symbol_index_remove_file(idx, file);

    // Extract symbols
    symbol_info_t *symbols = NULL;
    int count = 0;

    if (extract_symbols_from_source(source, file, &symbols, &count) != 0) {
        return -1;
    }

    // Add symbols to index
    for (int i = 0; i < count; i++) {
        symbol_info_t *sym = symbol_info_clone(&symbols[i]);
        if (sym) {
            symbol_index_add(idx, sym);
        }
    }

    free(symbols);
    return count;
}

// Utility functions
const char* symbol_kind_to_string(symbol_kind_t kind) {
    switch (kind) {
        case SYMBOL_FUNCTION: return "function";
        case SYMBOL_VARIABLE: return "variable";
        case SYMBOL_TYPEDEF: return "typedef";
        case SYMBOL_STRUCT: return "struct";
        case SYMBOL_ENUM: return "enum";
        case SYMBOL_MACRO: return "macro";
        default: return "unknown";
    }
}

symbol_kind_t symbol_kind_from_string(const char *kind_str) {
    if (!kind_str) return SYMBOL_UNKNOWN;
    if (strcmp(kind_str, "function") == 0) return SYMBOL_FUNCTION;
    if (strcmp(kind_str, "variable") == 0) return SYMBOL_VARIABLE;
    if (strcmp(kind_str, "typedef") == 0) return SYMBOL_TYPEDEF;
    if (strcmp(kind_str, "struct") == 0) return SYMBOL_STRUCT;
    if (strcmp(kind_str, "enum") == 0) return SYMBOL_ENUM;
    if (strcmp(kind_str, "macro") == 0) return SYMBOL_MACRO;
    return SYMBOL_UNKNOWN;
}

void symbol_info_print(const symbol_info_t *symbol) {
    if (!symbol) return;
    printf("Symbol: %s (%s) at %s:%d:%d\n",
           symbol->name,
           symbol_kind_to_string(symbol->kind),
           symbol->file,
           symbol->line,
           symbol->column);
    if (symbol->signature) {
        printf("  Signature: %s\n", symbol->signature);
    }
}

void symbol_index_dump(symbol_index_t *idx) {
    if (!idx) return;

    printf("Symbol Index: %d symbols\n", idx->count);
    for (int i = 0; i < SYMBOL_TABLE_SIZE; i++) {
        symbol_entry_t *entry = idx->table[i];
        while (entry) {
            symbol_info_print(entry->symbol);
            entry = entry->next;
        }
    }
}
