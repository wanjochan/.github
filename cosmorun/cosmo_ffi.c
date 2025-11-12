/* FFI Auto-Generator Implementation
 * Parses C headers and generates FFI bindings for dynamic loading
 */

#include "cosmo_ffi.h"
#include <ctype.h>
#include <stdarg.h>

/* Verbose logging */
static void ffi_log(const ffi_context_t *ctx, const char *fmt, ...) {
    if (ctx && ctx->options.verbose) {
        va_list args;
        va_start(args, fmt);
        vfprintf(stderr, fmt, args);
        va_end(args);
    }
}

/* Trim whitespace from string */
char *ffi_trim_whitespace(char *str) {
    char *end;

    if (!str) return NULL;

    /* Trim leading space */
    while (isspace((unsigned char)*str)) str++;

    if (*str == 0) return str;

    /* Trim trailing space */
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;

    /* Write new null terminator */
    end[1] = '\0';

    return str;
}

/* Check if line is a comment or empty */
int ffi_is_comment_or_empty(const char *line) {
    if (!line) return 1;

    const char *p = line;
    while (*p && isspace(*p)) p++;

    if (*p == '\0') return 1;
    if (p[0] == '/' && p[1] == '/') return 1;
    if (p[0] == '/' && p[1] == '*') return 1;
    if (*p == '#') return 1;  /* Preprocessor directive */

    return 0;
}

/* Remove preprocessor directives */
void ffi_remove_preprocessor(char *line) {
    if (!line) return;

    char *p = line;
    while (*p && isspace(*p)) p++;

    if (*p == '#') {
        *line = '\0';  /* Clear the line */
    }
}

/* Extract type and name from parameter like "int x" or "const char *str" */
static int parse_param_type_name(const char *param, char *type, char *name) {
    char buf[FFI_MAX_TYPE_LENGTH];
    strncpy(buf, param, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *p = ffi_trim_whitespace(buf);

    /* Find the last identifier (the name) */
    char *last_ident = NULL;
    char *current = p;

    while (*current) {
        while (*current && (isalnum(*current) || *current == '_')) {
            if (!last_ident) last_ident = current;
            current++;
        }
        if (*current && !isalnum(*current) && *current != '_') {
            last_ident = NULL;
            current++;
        }
    }

    if (!last_ident || last_ident == p) {
        /* No name found, might be just a type like "void" */
        strcpy(type, p);
        name[0] = '\0';
        return 1;
    }

    /* Extract name */
    strcpy(name, last_ident);

    /* Extract type (everything before the name) */
    size_t type_len = last_ident - p;
    strncpy(type, p, type_len);
    type[type_len] = '\0';
    ffi_trim_whitespace(type);

    return 1;
}

/* Determine type category from type string */
ffi_type_category_t ffi_get_type_category(const char *type_str) {
    if (!type_str) return FFI_TYPE_UNKNOWN;

    /* Check for pointer first (highest priority) */
    if (strstr(type_str, "*")) return FFI_TYPE_POINTER;

    /* Then check specific types */
    if (strstr(type_str, "struct")) return FFI_TYPE_STRUCT;
    if (strstr(type_str, "enum")) return FFI_TYPE_ENUM;
    if (strstr(type_str, "void")) return FFI_TYPE_VOID;
    if (strstr(type_str, "char")) return FFI_TYPE_CHAR;
    if (strstr(type_str, "short")) return FFI_TYPE_SHORT;
    if (strstr(type_str, "long long")) return FFI_TYPE_LONG_LONG;
    if (strstr(type_str, "long")) return FFI_TYPE_LONG;
    if (strstr(type_str, "int")) return FFI_TYPE_INT;
    if (strstr(type_str, "float")) return FFI_TYPE_FLOAT;
    if (strstr(type_str, "double")) return FFI_TYPE_DOUBLE;

    return FFI_TYPE_UNKNOWN;
}

/* Parse function declaration like "double sin(double x);" */
int ffi_parse_function_declaration(const char *line, ffi_function_t *func) {
    if (!line || !func) return 0;

    memset(func, 0, sizeof(ffi_function_t));

    /* Make a working copy */
    char buf[FFI_MAX_LINE_LENGTH];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* Remove semicolon */
    char *semi = strchr(buf, ';');
    if (semi) *semi = '\0';

    /* Find opening parenthesis */
    char *paren_open = strchr(buf, '(');
    if (!paren_open) return 0;

    /* Find closing parenthesis */
    char *paren_close = strrchr(buf, ')');
    if (!paren_close) return 0;

    *paren_open = '\0';
    *paren_close = '\0';

    /* Parse return type and function name */
    char *ret_and_name = ffi_trim_whitespace(buf);

    /* Find the function name (last identifier before '(') */
    char *func_name = NULL;
    char *p = ret_and_name;
    char *last_ident = NULL;

    while (*p) {
        if (isalnum(*p) || *p == '_') {
            if (!last_ident || p[-1] == ' ' || p[-1] == '*' || p[-1] == '\t') {
                last_ident = p;
            }
            p++;
        } else {
            p++;
        }
    }

    func_name = last_ident;
    if (!func_name) return 0;

    /* Extract function name */
    char *name_end = func_name;
    while (isalnum(*name_end) || *name_end == '_') name_end++;

    strncpy(func->name, func_name, name_end - func_name);
    func->name[name_end - func_name] = '\0';

    /* Extract return type */
    size_t ret_len = func_name - ret_and_name;
    strncpy(func->return_type, ret_and_name, ret_len);
    func->return_type[ret_len] = '\0';
    ffi_trim_whitespace(func->return_type);

    func->return_category = ffi_get_type_category(func->return_type);
    func->return_is_pointer = (strchr(func->return_type, '*') != NULL);

    /* Parse parameters */
    char *params_str = paren_open + 1;
    ffi_trim_whitespace(params_str);

    if (strcmp(params_str, "void") == 0 || strlen(params_str) == 0) {
        func->param_count = 0;
        return 1;
    }

    /* Check for variadic */
    if (strstr(params_str, "...")) {
        func->is_variadic = 1;
    }

    /* Split by comma */
    char *token = strtok(params_str, ",");
    while (token && func->param_count < FFI_MAX_PARAMS) {
        token = ffi_trim_whitespace(token);

        if (strcmp(token, "...") == 0) {
            func->is_variadic = 1;
            break;
        }

        ffi_param_t *param = &func->params[func->param_count];

        char type_buf[FFI_MAX_TYPE_LENGTH];
        char name_buf[FFI_MAX_NAME_LENGTH];

        parse_param_type_name(token, type_buf, name_buf);

        strncpy(param->type, type_buf, sizeof(param->type) - 1);
        strncpy(param->name, name_buf, sizeof(param->name) - 1);

        param->category = ffi_get_type_category(param->type);
        param->is_pointer = (strchr(param->type, '*') != NULL);
        param->is_const = (strstr(token, "const") != NULL);

        func->param_count++;
        token = strtok(NULL, ",");
    }

    return 1;
}

/* Parse typedef */
int ffi_parse_typedef(const char *line, ffi_typedef_t *td) {
    if (!line || !td) return 0;

    memset(td, 0, sizeof(ffi_typedef_t));

    /* Simple typedef parsing: "typedef old_type new_type;" */
    char buf[FFI_MAX_LINE_LENGTH];
    strncpy(buf, line, sizeof(buf) - 1);

    /* Remove "typedef" keyword */
    char *p = strstr(buf, "typedef");
    if (!p) return 0;
    p += 7;  /* Skip "typedef" */

    /* Remove semicolon */
    char *semi = strchr(p, ';');
    if (semi) *semi = '\0';

    p = ffi_trim_whitespace(p);

    /* Find last token (the alias) */
    char *last_space = strrchr(p, ' ');
    if (!last_space) return 0;

    strcpy(td->alias, ffi_trim_whitespace(last_space + 1));

    *last_space = '\0';
    strcpy(td->original, ffi_trim_whitespace(p));

    return 1;
}

/* Parse struct definition (simple version) */
int ffi_parse_struct(const char *text, ffi_struct_t *s) {
    if (!text || !s) return 0;

    memset(s, 0, sizeof(ffi_struct_t));

    /* Extract struct name from "struct name {" */
    const char *p = strstr(text, "struct");
    if (!p) return 0;

    p += 6;  /* Skip "struct" */
    while (*p && isspace(*p)) p++;

    /* Get struct name */
    const char *name_start = p;
    while (*p && (isalnum(*p) || *p == '_')) p++;

    size_t name_len = p - name_start;
    if (name_len > 0) {
        strncpy(s->name, name_start, name_len);
        s->name[name_len] = '\0';
    }

    return 1;
}

/* Parse enum definition (simple version) */
int ffi_parse_enum(const char *text, ffi_enum_t *e) {
    if (!text || !e) return 0;

    memset(e, 0, sizeof(ffi_enum_t));

    /* Extract enum name from "enum name {" */
    const char *p = strstr(text, "enum");
    if (!p) return 0;

    p += 4;  /* Skip "enum" */
    while (*p && isspace(*p)) p++;

    /* Get enum name */
    const char *name_start = p;
    while (*p && (isalnum(*p) || *p == '_')) p++;

    size_t name_len = p - name_start;
    if (name_len > 0) {
        strncpy(e->name, name_start, name_len);
        e->name[name_len] = '\0';
    }

    return 1;
}

/* Initialize FFI context */
ffi_context_t *ffi_context_create(const ffi_options_t *options) {
    ffi_context_t *ctx = (ffi_context_t *)calloc(1, sizeof(ffi_context_t));
    if (!ctx) return NULL;

    if (options) {
        ctx->options = *options;
    }

    return ctx;
}

/* Destroy FFI context */
void ffi_context_destroy(ffi_context_t *ctx) {
    if (ctx) {
        free(ctx);
    }
}

/* Parse C header file */
int ffi_parse_header(ffi_context_t *ctx, const char *header_path) {
    if (!ctx || !header_path) return 0;

    FILE *fp = fopen(header_path, "r");
    if (!fp) {
        fprintf(stderr, "Error: Cannot open header file: %s\n", header_path);
        return 0;
    }

    ffi_log(ctx, "Parsing header: %s\n", header_path);

    char line[FFI_MAX_LINE_LENGTH];
    int in_comment = 0;
    int line_num = 0;

    while (fgets(line, sizeof(line), fp)) {
        line_num++;

        /* Handle multi-line comments */
        if (strstr(line, "/*")) in_comment = 1;
        if (strstr(line, "*/")) {
            in_comment = 0;
            continue;
        }
        if (in_comment) continue;

        /* Skip empty lines and single-line comments */
        if (ffi_is_comment_or_empty(line)) continue;

        /* Remove preprocessor directives */
        ffi_remove_preprocessor(line);
        if (strlen(line) == 0) continue;

        /* Try to parse as typedef */
        if (strstr(line, "typedef") && ctx->typedef_count < FFI_MAX_TYPEDEFS) {
            ffi_typedef_t td;
            if (ffi_parse_typedef(line, &td)) {
                ctx->typedefs[ctx->typedef_count++] = td;
                ffi_log(ctx, "  Found typedef: %s -> %s\n", td.original, td.alias);
            }
        }

        /* Try to parse as struct */
        if (strstr(line, "struct") && strchr(line, '{') && ctx->struct_count < FFI_MAX_STRUCTS) {
            ffi_struct_t s;
            if (ffi_parse_struct(line, &s) && strlen(s.name) > 0) {
                ctx->structs[ctx->struct_count++] = s;
                ffi_log(ctx, "  Found struct: %s\n", s.name);
            }
        }

        /* Try to parse as enum */
        if (strstr(line, "enum") && strchr(line, '{') && ctx->enum_count < FFI_MAX_ENUMS) {
            ffi_enum_t e;
            if (ffi_parse_enum(line, &e) && strlen(e.name) > 0) {
                ctx->enums[ctx->enum_count++] = e;
                ffi_log(ctx, "  Found enum: %s\n", e.name);
            }
        }

        /* Try to parse as function declaration */
        if (strchr(line, '(') && strchr(line, ')') && strchr(line, ';')) {
            /* Skip if it looks like a macro */
            char *trimmed = ffi_trim_whitespace(line);
            if (trimmed[0] == '#') continue;

            if (ctx->function_count < FFI_MAX_FUNCTIONS) {
                ffi_function_t func;
                if (ffi_parse_function_declaration(line, &func)) {
                    /* Skip empty function names */
                    if (strlen(func.name) > 0) {
                        ctx->functions[ctx->function_count++] = func;
                        ffi_log(ctx, "  Found function: %s %s(...)\n",
                               func.return_type, func.name);
                    }
                }
            }
        }
    }

    fclose(fp);

    ffi_log(ctx, "Parsed: %d functions, %d structs, %d enums, %d typedefs\n",
           ctx->function_count, ctx->struct_count, ctx->enum_count, ctx->typedef_count);

    return 1;
}

/* Generate function pointer declaration */
void ffi_generate_function_pointer(FILE *out, const ffi_function_t *func) {
    if (!out || !func) return;

    /* Generate: return_type (*ffi_name)(params) = NULL; */
    fprintf(out, "%s (*ffi_%s)(", func->return_type, func->name);

    if (func->param_count == 0) {
        fprintf(out, "void");
    } else {
        for (int i = 0; i < func->param_count; i++) {
            fprintf(out, "%s", func->params[i].type);
            if (i < func->param_count - 1) {
                fprintf(out, ", ");
            }
        }
        if (func->is_variadic) {
            fprintf(out, ", ...");
        }
    }

    fprintf(out, ") = NULL;\n");
}

/* Generate dlsym loading code */
void ffi_generate_loader_code(FILE *out, const ffi_context_t *ctx) {
    if (!out || !ctx) return;

    const char *lib_name = ctx->options.library_name ? ctx->options.library_name : "library.so";

    fprintf(out, "\nint load_bindings(void) {\n");
    fprintf(out, "    void *lib = dlopen(\"%s\", RTLD_LAZY);\n", lib_name);
    fprintf(out, "    if (!lib) {\n");
    fprintf(out, "        fprintf(stderr, \"Error loading library: %%s\\n\", dlerror());\n");
    fprintf(out, "        return -1;\n");
    fprintf(out, "    }\n\n");

    for (int i = 0; i < ctx->function_count; i++) {
        const ffi_function_t *func = &ctx->functions[i];

        fprintf(out, "    ffi_%s = dlsym(lib, \"%s\");\n", func->name, func->name);

        if (ctx->options.add_error_checks) {
            fprintf(out, "    if (!ffi_%s) {\n", func->name);
            fprintf(out, "        fprintf(stderr, \"Error loading symbol %s: %%s\\n\", dlerror());\n",
                   func->name);
            fprintf(out, "    }\n");
        }
        fprintf(out, "\n");
    }

    fprintf(out, "    return 0;\n");
    fprintf(out, "}\n");
}

/* Generate bindings code */
int ffi_generate_bindings(ffi_context_t *ctx, const char *output_path) {
    if (!ctx) return 0;

    FILE *out = stdout;
    if (output_path) {
        out = fopen(output_path, "w");
        if (!out) {
            fprintf(stderr, "Error: Cannot open output file: %s\n", output_path);
            return 0;
        }
    }

    ffi_log(ctx, "Generating bindings to: %s\n", output_path ? output_path : "stdout");

    /* Generate header */
    fprintf(out, "/* Auto-generated FFI bindings */\n");
    fprintf(out, "/* Generated from: %s */\n\n",
           ctx->options.input_header ? ctx->options.input_header : "unknown");

    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "#include <dlfcn.h>\n\n");

    /* Generate function pointers */
    fprintf(out, "/* Function pointers */\n");
    for (int i = 0; i < ctx->function_count; i++) {
        ffi_generate_function_pointer(out, &ctx->functions[i]);
    }

    /* Generate loader function if requested */
    if (ctx->options.generate_loader) {
        ffi_generate_loader_code(out, ctx);
    }

    /* Generate example usage */
    fprintf(out, "\n/* Example usage:\n");
    fprintf(out, " * int main() {\n");
    fprintf(out, " *     if (load_bindings() != 0) {\n");
    fprintf(out, " *         return 1;\n");
    fprintf(out, " *     }\n");
    fprintf(out, " *     \n");
    if (ctx->function_count > 0) {
        const ffi_function_t *func = &ctx->functions[0];
        fprintf(out, " *     // Call: ffi_%s(...);\n", func->name);
    }
    fprintf(out, " *     return 0;\n");
    fprintf(out, " * }\n");
    fprintf(out, " */\n");

    if (output_path) {
        fclose(out);
    }

    ffi_log(ctx, "Generated bindings for %d functions\n", ctx->function_count);

    return 1;
}
