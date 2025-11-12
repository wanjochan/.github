// LSP (Language Server Protocol) server for code navigation
// Provides LSP method handlers for go-to-definition, find-references, etc.

#ifndef COSMO_LSP_H
#define COSMO_LSP_H

#include "cosmo_symbols.h"

// LSP server state
typedef struct lsp_server_t lsp_server_t;

// LSP position (LSP uses 0-based line and character)
typedef struct {
    int line;
    int character;
} lsp_position_t;

// LSP range
typedef struct {
    lsp_position_t start;
    lsp_position_t end;
} lsp_range_t;

// LSP location
typedef struct {
    char *uri;
    lsp_range_t range;
} lsp_location_t;

// LSP document symbol
typedef struct {
    char *name;
    int kind;  // LSP SymbolKind
    lsp_range_t range;
    lsp_range_t selection_range;
} lsp_document_symbol_t;

// LSP server lifecycle
lsp_server_t* lsp_server_create(void);
void lsp_server_destroy(lsp_server_t *lsp);
int lsp_server_run(lsp_server_t *lsp);  // Main loop

// LSP standard methods
int lsp_handle_initialize(lsp_server_t *lsp, const char *params, char *response, int max_len);
int lsp_handle_shutdown(lsp_server_t *lsp, const char *params, char *response, int max_len);
int lsp_handle_exit(lsp_server_t *lsp);

// Document synchronization
int lsp_handle_did_open(lsp_server_t *lsp, const char *params);
int lsp_handle_did_change(lsp_server_t *lsp, const char *params);
int lsp_handle_did_close(lsp_server_t *lsp, const char *params);

// Navigation methods
int lsp_handle_goto_definition(lsp_server_t *lsp, const char *params, char *response, int max_len);
int lsp_handle_find_references(lsp_server_t *lsp, const char *params, char *response, int max_len);
int lsp_handle_document_symbol(lsp_server_t *lsp, const char *params, char *response, int max_len);

// Utility functions
int lsp_read_request(char *buffer, int max_len);
int lsp_send_response(const char *response);
int lsp_send_error(int id, int code, const char *message);

// JSON helpers
int lsp_parse_position(const char *json, lsp_position_t *pos);
int lsp_parse_text_document(const char *json, char *uri, int uri_max_len);
int lsp_format_location(const lsp_location_t *loc, char *json, int max_len);
int lsp_format_locations(const lsp_location_t *locs, int count, char *json, int max_len);
int lsp_format_symbol(const lsp_document_symbol_t *sym, char *json, int max_len);
int lsp_format_symbols(const lsp_document_symbol_t *syms, int count, char *json, int max_len);

// URI helpers
int lsp_uri_to_path(const char *uri, char *path, int max_len);
int lsp_path_to_uri(const char *path, char *uri, int max_len);

// Symbol kind mapping (LSP SymbolKind)
enum {
    LSP_SYMBOL_FILE = 1,
    LSP_SYMBOL_MODULE = 2,
    LSP_SYMBOL_NAMESPACE = 3,
    LSP_SYMBOL_PACKAGE = 4,
    LSP_SYMBOL_CLASS = 5,
    LSP_SYMBOL_METHOD = 6,
    LSP_SYMBOL_PROPERTY = 7,
    LSP_SYMBOL_FIELD = 8,
    LSP_SYMBOL_CONSTRUCTOR = 9,
    LSP_SYMBOL_ENUM = 10,
    LSP_SYMBOL_INTERFACE = 11,
    LSP_SYMBOL_FUNCTION = 12,
    LSP_SYMBOL_VARIABLE = 13,
    LSP_SYMBOL_CONSTANT = 14,
    LSP_SYMBOL_STRING = 15,
    LSP_SYMBOL_NUMBER = 16,
    LSP_SYMBOL_BOOLEAN = 17,
    LSP_SYMBOL_ARRAY = 18,
    LSP_SYMBOL_OBJECT = 19,
    LSP_SYMBOL_KEY = 20,
    LSP_SYMBOL_NULL = 21,
    LSP_SYMBOL_ENUM_MEMBER = 22,
    LSP_SYMBOL_STRUCT = 23,
    LSP_SYMBOL_EVENT = 24,
    LSP_SYMBOL_OPERATOR = 25,
    LSP_SYMBOL_TYPE_PARAMETER = 26
};

int symbol_kind_to_lsp_kind(symbol_kind_t kind);

// Error codes (LSP ErrorCodes)
enum {
    LSP_ERROR_PARSE_ERROR = -32700,
    LSP_ERROR_INVALID_REQUEST = -32600,
    LSP_ERROR_METHOD_NOT_FOUND = -32601,
    LSP_ERROR_INVALID_PARAMS = -32602,
    LSP_ERROR_INTERNAL_ERROR = -32603,
    LSP_ERROR_SERVER_NOT_INITIALIZED = -32002,
    LSP_ERROR_UNKNOWN_ERROR_CODE = -32001,
    LSP_ERROR_REQUEST_FAILED = -32803,
    LSP_ERROR_SERVER_CANCELLED = -32802,
    LSP_ERROR_CONTENT_MODIFIED = -32801,
    LSP_ERROR_REQUEST_CANCELLED = -32800
};

#endif // COSMO_LSP_H
