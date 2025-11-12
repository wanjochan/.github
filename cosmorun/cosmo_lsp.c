// LSP server implementation
#include "cosmo_lsp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_REQUEST_SIZE 65536
#define MAX_RESPONSE_SIZE 65536
#define MAX_URI_LENGTH 2048
#define MAX_PATH_LENGTH 2048

// LSP server structure
struct lsp_server_t {
    symbol_index_t *symbols;
    int initialized;
    int shutdown_requested;
};

// Simple JSON parsing helpers
static const char* find_json_string(const char *json, const char *key, char *value, int max_len) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);

    const char *pos = strstr(json, search);
    if (!pos) return NULL;

    pos += strlen(search);
    while (*pos && (*pos == ':' || isspace(*pos))) pos++;

    if (*pos != '"') return NULL;
    pos++;

    int i = 0;
    while (*pos && *pos != '"' && i < max_len - 1) {
        if (*pos == '\\' && *(pos + 1)) {
            pos++;
        }
        value[i++] = *pos++;
    }
    value[i] = '\0';

    return pos;
}

static int find_json_int(const char *json, const char *key, int default_value) {
    char search[256];
    snprintf(search, sizeof(search), "\"%s\"", key);

    const char *pos = strstr(json, search);
    if (!pos) return default_value;

    pos += strlen(search);
    while (*pos && (*pos == ':' || isspace(*pos))) pos++;

    return atoi(pos);
}

// LSP server lifecycle
lsp_server_t* lsp_server_create(void) {
    lsp_server_t *lsp = (lsp_server_t*)malloc(sizeof(lsp_server_t));
    if (!lsp) return NULL;

    lsp->symbols = symbol_index_create();
    if (!lsp->symbols) {
        free(lsp);
        return NULL;
    }

    lsp->initialized = 0;
    lsp->shutdown_requested = 0;

    return lsp;
}

void lsp_server_destroy(lsp_server_t *lsp) {
    if (!lsp) return;

    if (lsp->symbols) {
        symbol_index_destroy(lsp->symbols);
    }

    free(lsp);
}

// I/O functions
int lsp_read_request(char *buffer, int max_len) {
    // Read Content-Length header
    char header[256];
    if (!fgets(header, sizeof(header), stdin)) {
        return -1;
    }

    int content_length = 0;
    if (sscanf(header, "Content-Length: %d", &content_length) != 1) {
        return -1;
    }

    if (content_length <= 0 || content_length >= max_len) {
        return -1;
    }

    // Skip empty line
    if (!fgets(header, sizeof(header), stdin)) {
        return -1;
    }

    // Read content
    size_t bytes_read = fread(buffer, 1, content_length, stdin);
    if (bytes_read != (size_t)content_length) {
        return -1;
    }

    buffer[content_length] = '\0';
    return content_length;
}

int lsp_send_response(const char *response) {
    if (!response) return -1;

    int content_length = strlen(response);
    printf("Content-Length: %d\r\n\r\n%s", content_length, response);
    fflush(stdout);

    return 0;
}

int lsp_send_error(int id, int code, const char *message) {
    char response[4096];
    snprintf(response, sizeof(response),
             "{\"jsonrpc\":\"2.0\",\"id\":%d,\"error\":{\"code\":%d,\"message\":\"%s\"}}",
             id, code, message ? message : "Unknown error");

    return lsp_send_response(response);
}

// URI helpers
int lsp_uri_to_path(const char *uri, char *path, int max_len) {
    if (!uri || !path) return -1;

    // Handle file:// URI
    if (strncmp(uri, "file://", 7) == 0) {
        uri += 7;

        // Handle Windows paths (file:///C:/...)
        #ifdef _WIN32
        if (*uri == '/') uri++;
        #endif
    }

    int i = 0;
    while (*uri && i < max_len - 1) {
        // Decode percent-encoded characters
        if (*uri == '%' && isxdigit(uri[1]) && isxdigit(uri[2])) {
            char hex[3] = {uri[1], uri[2], 0};
            path[i++] = (char)strtol(hex, NULL, 16);
            uri += 3;
        } else {
            path[i++] = *uri++;
        }
    }
    path[i] = '\0';

    return 0;
}

int lsp_path_to_uri(const char *path, char *uri, int max_len) {
    if (!path || !uri) return -1;

    int len = snprintf(uri, max_len, "file://");

    #ifdef _WIN32
    if (len < max_len) uri[len++] = '/';
    #endif

    while (*path && len < max_len - 1) {
        if (isalnum(*path) || *path == '/' || *path == '.' || *path == '-' || *path == '_') {
            uri[len++] = *path;
        } else {
            len += snprintf(uri + len, max_len - len, "%%%02X", (unsigned char)*path);
        }
        path++;
    }

    uri[len] = '\0';
    return 0;
}

// JSON parsing
int lsp_parse_position(const char *json, lsp_position_t *pos) {
    if (!json || !pos) return -1;

    pos->line = find_json_int(json, "line", -1);
    pos->character = find_json_int(json, "character", -1);

    return (pos->line >= 0 && pos->character >= 0) ? 0 : -1;
}

int lsp_parse_text_document(const char *json, char *uri, int uri_max_len) {
    if (!json || !uri) return -1;

    const char *td = strstr(json, "\"textDocument\"");
    if (!td) return -1;

    return find_json_string(td, "uri", uri, uri_max_len) ? 0 : -1;
}

// JSON formatting
int lsp_format_location(const lsp_location_t *loc, char *json, int max_len) {
    if (!loc || !json) return -1;

    return snprintf(json, max_len,
                    "{\"uri\":\"%s\",\"range\":{\"start\":{\"line\":%d,\"character\":%d},\"end\":{\"line\":%d,\"character\":%d}}}",
                    loc->uri,
                    loc->range.start.line,
                    loc->range.start.character,
                    loc->range.end.line,
                    loc->range.end.character);
}

int lsp_format_locations(const lsp_location_t *locs, int count, char *json, int max_len) {
    if (!locs || !json || count <= 0) return -1;

    int len = snprintf(json, max_len, "[");

    for (int i = 0; i < count && len < max_len; i++) {
        if (i > 0) len += snprintf(json + len, max_len - len, ",");

        char loc_json[1024];
        lsp_format_location(&locs[i], loc_json, sizeof(loc_json));
        len += snprintf(json + len, max_len - len, "%s", loc_json);
    }

    len += snprintf(json + len, max_len - len, "]");
    return len;
}

int lsp_format_symbol(const lsp_document_symbol_t *sym, char *json, int max_len) {
    if (!sym || !json) return -1;

    return snprintf(json, max_len,
                    "{\"name\":\"%s\",\"kind\":%d,\"range\":{\"start\":{\"line\":%d,\"character\":%d},\"end\":{\"line\":%d,\"character\":%d}},\"selectionRange\":{\"start\":{\"line\":%d,\"character\":%d},\"end\":{\"line\":%d,\"character\":%d}}}",
                    sym->name,
                    sym->kind,
                    sym->range.start.line,
                    sym->range.start.character,
                    sym->range.end.line,
                    sym->range.end.character,
                    sym->selection_range.start.line,
                    sym->selection_range.start.character,
                    sym->selection_range.end.line,
                    sym->selection_range.end.character);
}

int lsp_format_symbols(const lsp_document_symbol_t *syms, int count, char *json, int max_len) {
    if (!syms || !json || count <= 0) return -1;

    int len = snprintf(json, max_len, "[");

    for (int i = 0; i < count && len < max_len; i++) {
        if (i > 0) len += snprintf(json + len, max_len - len, ",");

        char sym_json[1024];
        lsp_format_symbol(&syms[i], sym_json, sizeof(sym_json));
        len += snprintf(json + len, max_len - len, "%s", sym_json);
    }

    len += snprintf(json + len, max_len - len, "]");
    return len;
}

// Symbol kind mapping
int symbol_kind_to_lsp_kind(symbol_kind_t kind) {
    switch (kind) {
        case SYMBOL_FUNCTION: return LSP_SYMBOL_FUNCTION;
        case SYMBOL_VARIABLE: return LSP_SYMBOL_VARIABLE;
        case SYMBOL_TYPEDEF: return LSP_SYMBOL_TYPE_PARAMETER;
        case SYMBOL_STRUCT: return LSP_SYMBOL_STRUCT;
        case SYMBOL_ENUM: return LSP_SYMBOL_ENUM;
        case SYMBOL_MACRO: return LSP_SYMBOL_CONSTANT;
        default: return LSP_SYMBOL_VARIABLE;
    }
}

// LSP method handlers
int lsp_handle_initialize(lsp_server_t *lsp, const char *params, char *response, int max_len) {
    if (!lsp || !response) return -1;

    lsp->initialized = 1;

    snprintf(response, max_len,
             "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":{\"capabilities\":{"
             "\"textDocumentSync\":1,"
             "\"definitionProvider\":true,"
             "\"referencesProvider\":true,"
             "\"documentSymbolProvider\":true}}}");

    return 0;
}

int lsp_handle_shutdown(lsp_server_t *lsp, const char *params, char *response, int max_len) {
    if (!lsp || !response) return -1;

    lsp->shutdown_requested = 1;
    snprintf(response, max_len, "{\"jsonrpc\":\"2.0\",\"id\":null,\"result\":null}");

    return 0;
}

int lsp_handle_exit(lsp_server_t *lsp) {
    if (!lsp) return -1;
    return 0;
}

int lsp_handle_did_open(lsp_server_t *lsp, const char *params) {
    if (!lsp || !params) return -1;

    char uri[MAX_URI_LENGTH];
    if (lsp_parse_text_document(params, uri, sizeof(uri)) != 0) {
        return -1;
    }

    char path[MAX_PATH_LENGTH];
    if (lsp_uri_to_path(uri, path, sizeof(path)) != 0) {
        return -1;
    }

    // Extract text content
    char text[MAX_REQUEST_SIZE];
    const char *text_field = strstr(params, "\"text\"");
    if (text_field && find_json_string(text_field, "text", text, sizeof(text))) {
        symbol_index_parse_file(lsp->symbols, path, text);
    }

    return 0;
}

int lsp_handle_did_change(lsp_server_t *lsp, const char *params) {
    // Re-parse file on change
    return lsp_handle_did_open(lsp, params);
}

int lsp_handle_did_close(lsp_server_t *lsp, const char *params) {
    if (!lsp || !params) return -1;

    char uri[MAX_URI_LENGTH];
    if (lsp_parse_text_document(params, uri, sizeof(uri)) != 0) {
        return -1;
    }

    char path[MAX_PATH_LENGTH];
    if (lsp_uri_to_path(uri, path, sizeof(path)) != 0) {
        return -1;
    }

    symbol_index_remove_file(lsp->symbols, path);
    return 0;
}

int lsp_handle_goto_definition(lsp_server_t *lsp, const char *params, char *response, int max_len) {
    if (!lsp || !params || !response) return -1;

    // Parse request
    char uri[MAX_URI_LENGTH];
    lsp_position_t pos;

    if (lsp_parse_text_document(params, uri, sizeof(uri)) != 0) {
        return -1;
    }

    const char *pos_field = strstr(params, "\"position\"");
    if (!pos_field || lsp_parse_position(pos_field, &pos) != 0) {
        return -1;
    }

    // Convert URI to path
    char path[MAX_PATH_LENGTH];
    if (lsp_uri_to_path(uri, path, sizeof(path)) != 0) {
        return -1;
    }

    // Find symbol at position (LSP uses 0-based lines, we use 1-based)
    symbol_info_t *sym = symbol_index_find_at_position(lsp->symbols, path, pos.line + 1, pos.character);
    if (!sym) {
        snprintf(response, max_len, "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":null}");
        return 0;
    }

    // Find definition
    symbol_info_t *def = symbol_index_find_definition(lsp->symbols, sym->name);
    if (!def) {
        snprintf(response, max_len, "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":null}");
        return 0;
    }

    // Format response
    char def_uri[MAX_URI_LENGTH];
    lsp_path_to_uri(def->file, def_uri, sizeof(def_uri));

    lsp_location_t loc;
    loc.uri = def_uri;
    loc.range.start.line = def->line - 1;  // Convert to 0-based
    loc.range.start.character = def->column;
    loc.range.end.line = def->line - 1;
    loc.range.end.character = def->column + strlen(def->name);

    char loc_json[2048];
    lsp_format_location(&loc, loc_json, sizeof(loc_json));

    snprintf(response, max_len, "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":%s}", loc_json);
    return 0;
}

int lsp_handle_find_references(lsp_server_t *lsp, const char *params, char *response, int max_len) {
    if (!lsp || !params || !response) return -1;

    // Parse request
    char uri[MAX_URI_LENGTH];
    lsp_position_t pos;

    if (lsp_parse_text_document(params, uri, sizeof(uri)) != 0) {
        return -1;
    }

    const char *pos_field = strstr(params, "\"position\"");
    if (!pos_field || lsp_parse_position(pos_field, &pos) != 0) {
        return -1;
    }

    // Convert URI to path
    char path[MAX_PATH_LENGTH];
    if (lsp_uri_to_path(uri, path, sizeof(path)) != 0) {
        return -1;
    }

    // Find symbol at position
    symbol_info_t *sym = symbol_index_find_at_position(lsp->symbols, path, pos.line + 1, pos.character);
    if (!sym) {
        snprintf(response, max_len, "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":[]}");
        return 0;
    }

    // Find references
    int ref_count = 0;
    symbol_location_t *refs = symbol_index_find_references(lsp->symbols, sym->name, &ref_count);

    if (!refs || ref_count == 0) {
        snprintf(response, max_len, "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":[]}");
        return 0;
    }

    // Convert to LSP locations
    lsp_location_t *lsp_locs = (lsp_location_t*)malloc(ref_count * sizeof(lsp_location_t));
    if (!lsp_locs) {
        free(refs);
        return -1;
    }

    for (int i = 0; i < ref_count; i++) {
        char ref_uri[MAX_URI_LENGTH];
        lsp_path_to_uri(refs[i].file, ref_uri, sizeof(ref_uri));

        lsp_locs[i].uri = strdup(ref_uri);
        lsp_locs[i].range.start.line = refs[i].line - 1;
        lsp_locs[i].range.start.character = refs[i].column;
        lsp_locs[i].range.end.line = refs[i].line - 1;
        lsp_locs[i].range.end.character = refs[i].column + strlen(sym->name);
    }

    // Format response
    char locs_json[MAX_RESPONSE_SIZE];
    lsp_format_locations(lsp_locs, ref_count, locs_json, sizeof(locs_json));

    snprintf(response, max_len, "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":%s}", locs_json);

    // Cleanup
    for (int i = 0; i < ref_count; i++) {
        free(lsp_locs[i].uri);
    }
    free(lsp_locs);
    free(refs);

    return 0;
}

int lsp_handle_document_symbol(lsp_server_t *lsp, const char *params, char *response, int max_len) {
    if (!lsp || !params || !response) return -1;

    // Parse request
    char uri[MAX_URI_LENGTH];
    if (lsp_parse_text_document(params, uri, sizeof(uri)) != 0) {
        return -1;
    }

    // Convert URI to path
    char path[MAX_PATH_LENGTH];
    if (lsp_uri_to_path(uri, path, sizeof(path)) != 0) {
        return -1;
    }

    // Get file symbols
    int sym_count = 0;
    symbol_info_t *symbols = symbol_index_list_file_symbols(lsp->symbols, path, &sym_count);

    if (!symbols || sym_count == 0) {
        snprintf(response, max_len, "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":[]}");
        return 0;
    }

    // Convert to LSP document symbols
    lsp_document_symbol_t *lsp_syms = (lsp_document_symbol_t*)malloc(sym_count * sizeof(lsp_document_symbol_t));
    if (!lsp_syms) {
        free(symbols);
        return -1;
    }

    for (int i = 0; i < sym_count; i++) {
        lsp_syms[i].name = symbols[i].name;
        lsp_syms[i].kind = symbol_kind_to_lsp_kind(symbols[i].kind);
        lsp_syms[i].range.start.line = symbols[i].line - 1;
        lsp_syms[i].range.start.character = symbols[i].column;
        lsp_syms[i].range.end.line = symbols[i].line - 1;
        lsp_syms[i].range.end.character = symbols[i].column + strlen(symbols[i].name);
        lsp_syms[i].selection_range = lsp_syms[i].range;
    }

    // Format response
    char syms_json[MAX_RESPONSE_SIZE];
    lsp_format_symbols(lsp_syms, sym_count, syms_json, sizeof(syms_json));

    snprintf(response, max_len, "{\"jsonrpc\":\"2.0\",\"id\":1,\"result\":%s}", syms_json);

    // Cleanup
    free(lsp_syms);
    free(symbols);

    return 0;
}

// Main server loop
int lsp_server_run(lsp_server_t *lsp) {
    if (!lsp) return -1;

    char request[MAX_REQUEST_SIZE];
    char response[MAX_RESPONSE_SIZE];

    while (!lsp->shutdown_requested) {
        // Read request
        int len = lsp_read_request(request, sizeof(request));
        if (len <= 0) {
            break;
        }

        // Parse method
        char method[256];
        if (!find_json_string(request, "method", method, sizeof(method))) {
            continue;
        }

        // Dispatch
        if (strcmp(method, "initialize") == 0) {
            lsp_handle_initialize(lsp, request, response, sizeof(response));
            lsp_send_response(response);
        } else if (strcmp(method, "shutdown") == 0) {
            lsp_handle_shutdown(lsp, request, response, sizeof(response));
            lsp_send_response(response);
        } else if (strcmp(method, "exit") == 0) {
            lsp_handle_exit(lsp);
            break;
        } else if (strcmp(method, "textDocument/didOpen") == 0) {
            lsp_handle_did_open(lsp, request);
        } else if (strcmp(method, "textDocument/didChange") == 0) {
            lsp_handle_did_change(lsp, request);
        } else if (strcmp(method, "textDocument/didClose") == 0) {
            lsp_handle_did_close(lsp, request);
        } else if (strcmp(method, "textDocument/definition") == 0) {
            lsp_handle_goto_definition(lsp, request, response, sizeof(response));
            lsp_send_response(response);
        } else if (strcmp(method, "textDocument/references") == 0) {
            lsp_handle_find_references(lsp, request, response, sizeof(response));
            lsp_send_response(response);
        } else if (strcmp(method, "textDocument/documentSymbol") == 0) {
            lsp_handle_document_symbol(lsp, request, response, sizeof(response));
            lsp_send_response(response);
        }
    }

    return 0;
}
