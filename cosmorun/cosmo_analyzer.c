/**
 * @file cosmo_analyzer.c
 * @brief Static Code Analysis Implementation
 */

#include "cosmo_analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>

/* Symbol tracking for analysis */
typedef struct Symbol {
    char name[128];
    int line;
    int is_function;
    int is_declared;
    int is_defined;
    int is_used;
    int read_count;
    int write_count;
    struct Symbol *next;
} Symbol;

typedef struct {
    Symbol *symbols;
    AnalysisIssue *issues;
    int issue_count;
    const char *current_file;
    int current_line;
} AnalysisContext;

/* Helper functions */
static void add_issue(AnalysisContext *ctx, IssueType type, IssueSeverity severity,
                     int line, const char *fmt, ...);
static Symbol* find_symbol(AnalysisContext *ctx, const char *name);
static Symbol* add_symbol(AnalysisContext *ctx, const char *name, int line, int is_function);
static void mark_symbol_used(AnalysisContext *ctx, const char *name);
static int is_keyword(const char *word);
static int is_type_keyword(const char *word);
static int is_type_keyword_at(const char *p);

/* Initialize default analysis options */
void init_default_analysis_options(AnalysisOptions *options) {
    options->verbose = 0;
    options->check_dead_code = 1;
    options->check_unused_vars = 1;
    options->check_type_safety = 1;
    options->check_null_deref = 1;
    options->check_unreachable = 1;
    options->check_memory_leaks = 1;
    options->check_local_unused = 1;
    options->check_uninitialized = 1;
}

/* Add an analysis issue */
static void add_issue(AnalysisContext *ctx, IssueType type, IssueSeverity severity,
                     int line, const char *fmt, ...) {
    AnalysisIssue *issue = (AnalysisIssue*)calloc(1, sizeof(AnalysisIssue));
    if (!issue) return;

    issue->type = type;
    issue->severity = severity;
    issue->file = ctx->current_file;
    issue->line = line;
    issue->column = 0;

    va_list args;
    va_start(args, fmt);
    vsnprintf(issue->message, sizeof(issue->message), fmt, args);
    va_end(args);

    /* Prepend to list */
    issue->next = ctx->issues;
    ctx->issues = issue;
    ctx->issue_count++;
}

/* Find symbol by name */
static Symbol* find_symbol(AnalysisContext *ctx, const char *name) {
    for (Symbol *s = ctx->symbols; s; s = s->next) {
        if (strcmp(s->name, name) == 0) {
            return s;
        }
    }
    return NULL;
}

/* Add symbol to context */
static Symbol* add_symbol(AnalysisContext *ctx, const char *name, int line, int is_function) {
    Symbol *sym = (Symbol*)calloc(1, sizeof(Symbol));
    if (!sym) return NULL;

    strncpy(sym->name, name, sizeof(sym->name) - 1);
    sym->line = line;
    sym->is_function = is_function;
    sym->is_declared = 1;
    sym->next = ctx->symbols;
    ctx->symbols = sym;
    return sym;
}

/* Mark symbol as used */
static void mark_symbol_used(AnalysisContext *ctx, const char *name) {
    Symbol *sym = find_symbol(ctx, name);
    if (sym) {
        sym->is_used = 1;
        sym->read_count++;
    }
}

/* Check if word is a C keyword */
static int is_keyword(const char *word) {
    static const char *keywords[] = {
        "auto", "break", "case", "char", "const", "continue", "default", "do",
        "double", "else", "enum", "extern", "float", "for", "goto", "if",
        "inline", "int", "long", "register", "restrict", "return", "short",
        "signed", "sizeof", "static", "struct", "switch", "typedef", "union",
        "unsigned", "void", "volatile", "while", "_Bool", "_Complex", "_Imaginary",
        NULL
    };
    for (int i = 0; keywords[i]; i++) {
        if (strcmp(word, keywords[i]) == 0) return 1;
    }
    return 0;
}

/* Check if word is a type keyword */
static int is_type_keyword(const char *word) {
    static const char *types[] = {
        "char", "short", "int", "long", "float", "double", "void",
        "signed", "unsigned", "struct", "union", "enum", "_Bool",
        NULL
    };
    for (int i = 0; types[i]; i++) {
        if (strcmp(word, types[i]) == 0) return 1;
    }
    return 0;
}

/* Simple tokenizer */
typedef struct {
    const char *start;
    const char *current;
    int line;
} Tokenizer;

static void init_tokenizer(Tokenizer *tok, const char *source) {
    tok->start = source;
    tok->current = source;
    tok->line = 1;
}

static void skip_whitespace_and_comments(Tokenizer *tok) {
    while (*tok->current) {
        if (*tok->current == ' ' || *tok->current == '\t' || *tok->current == '\r') {
            tok->current++;
        } else if (*tok->current == '\n') {
            tok->line++;
            tok->current++;
        } else if (*tok->current == '/' && *(tok->current + 1) == '/') {
            /* Single-line comment */
            tok->current += 2;
            while (*tok->current && *tok->current != '\n') {
                tok->current++;
            }
        } else if (*tok->current == '/' && *(tok->current + 1) == '*') {
            /* Multi-line comment */
            tok->current += 2;
            while (*tok->current && !(*tok->current == '*' && *(tok->current + 1) == '/')) {
                if (*tok->current == '\n') tok->line++;
                tok->current++;
            }
            if (*tok->current) tok->current += 2;
        } else {
            break;
        }
    }
}

static int is_identifier_char(char c, int first) {
    if (first) {
        return isalpha(c) || c == '_';
    }
    return isalnum(c) || c == '_';
}

static char* get_identifier(Tokenizer *tok) {
    skip_whitespace_and_comments(tok);
    if (!is_identifier_char(*tok->current, 1)) return NULL;

    const char *start = tok->current;
    while (is_identifier_char(*tok->current, 0)) {
        tok->current++;
    }

    int len = tok->current - start;
    char *id = (char*)malloc(len + 1);
    if (id) {
        memcpy(id, start, len);
        id[len] = '\0';
    }
    return id;
}

/* Parse function declaration/definition - called when we see identifier followed by '(' */
static void parse_function(AnalysisContext *ctx, const char *name, int line, Tokenizer *tok) {
    /* Found function name followed by '(' */
    Symbol *sym = find_symbol(ctx, name);
    if (!sym) {
        sym = add_symbol(ctx, name, line, 1);
    }

    /* Skip parameter list */
    int paren_depth = 1;
    tok->current++;  /* Skip opening '(' */
    while (*tok->current && paren_depth > 0) {
        if (*tok->current == '(') paren_depth++;
        else if (*tok->current == ')') paren_depth--;
        if (*tok->current == '\n') tok->line++;
        tok->current++;
    }

    /* Check if it's a definition (has body) or just declaration */
    skip_whitespace_and_comments(tok);
    if (*tok->current == '{') {
        sym->is_defined = 1;
        /* Skip function body */
        int brace_depth = 1;
        tok->current++;
        while (*tok->current && brace_depth > 0) {
            if (*tok->current == '{') brace_depth++;
            else if (*tok->current == '}') brace_depth--;
            if (*tok->current == '\n') tok->line++;
            tok->current++;
        }
    }
}

/* Parse variable declaration */
static void parse_variable(AnalysisContext *ctx, Tokenizer *tok, const char *var_name) {
    int line = tok->line;

    /* Skip common system/library variables */
    if (strncmp(var_name, "__", 2) == 0 ||
        strcmp(var_name, "argc") == 0 || strcmp(var_name, "argv") == 0 ||
        strcmp(var_name, "environ") == 0) {
        return;
    }

    Symbol *sym = find_symbol(ctx, var_name);
    if (!sym) {
        sym = add_symbol(ctx, var_name, line, 0);
    }
}

/* Scan for function calls and variable usage - only in function bodies */
static void scan_usage(AnalysisContext *ctx, const char *source) {
    Tokenizer tok;
    init_tokenizer(&tok, source);
    int in_function_body = 0;
    int brace_depth = 0;

    while (*tok.current) {
        skip_whitespace_and_comments(&tok);
        if (!*tok.current) break;

        /* Track brace depth to know when we're in function body */
        if (*tok.current == '{') {
            brace_depth++;
            if (brace_depth == 1) {
                in_function_body = 1;
            }
            tok.current++;
            continue;
        } else if (*tok.current == '}') {
            brace_depth--;
            if (brace_depth == 0) {
                in_function_body = 0;
            }
            tok.current++;
            continue;
        }

        /* Only mark usage inside function bodies */
        if (in_function_body && brace_depth > 0) {
            char *id = get_identifier(&tok);
            if (id) {
                /* Don't count keywords */
                if (!is_keyword(id) && !is_type_keyword(id)) {
                    Symbol *sym = find_symbol(ctx, id);
                    if (sym) {
                        mark_symbol_used(ctx, id);
                    }
                }
                free(id);
            } else {
                tok.current++;
            }
        } else {
            char *id = get_identifier(&tok);
            if (id) free(id);
            else tok.current++;
        }
    }
}

/* Main analysis pass - simpler approach */
static void analyze_pass1_declarations(AnalysisContext *ctx, const char *source) {
    Tokenizer tok;
    init_tokenizer(&tok, source);

    char *prev_id = NULL;
    int prev_line = 1;

    while (*tok.current) {
        skip_whitespace_and_comments(&tok);
        if (!*tok.current) break;

        ctx->current_line = tok.line;

        /* Get identifier */
        char *id = get_identifier(&tok);
        if (id) {
            int id_line = tok.line;
            skip_whitespace_and_comments(&tok);

            if (*tok.current == '(') {
                /* This is a function - check if previous token was a type */
                if (prev_id && !is_keyword(id)) {
                    parse_function(ctx, id, id_line, &tok);
                }
                free(id);
                if (prev_id) {
                    free(prev_id);
                    prev_id = NULL;
                }
            } else {
                /* Check if previous was a type keyword and this is a variable */
                if (prev_id && is_type_keyword(prev_id) && !is_keyword(id)) {
                    /* Handle pointer stars between type and name */
                    if (*tok.current == '=' || *tok.current == ';' || *tok.current == ',' || *tok.current == '[') {
                        parse_variable(ctx, &tok, id);
                    }
                }

                if (prev_id) free(prev_id);
                prev_id = id;
                prev_line = id_line;
            }
        } else {
            if (prev_id) free(prev_id);
            prev_id = NULL;
            tok.current++;
        }
    }

    if (prev_id) free(prev_id);
}

/* Check for type safety issues (implicit int<->pointer conversions) */
static void check_type_safety(AnalysisContext *ctx, const char *source) {
    const char *p = source;
    int line = 1;
    int in_function = 0;
    int brace_depth = 0;

    while (*p) {
        if (*p == '\n') line++;

        if (*p == '{') {
            brace_depth++;
            if (brace_depth == 1) in_function = 1;
        } else if (*p == '}') {
            brace_depth--;
            if (brace_depth == 0) in_function = 0;
        }

        if (in_function && brace_depth > 0) {
            /* Look for (void*)integer_var or int = pointer patterns */
            if (*p == '(' && strncmp(p+1, "void", 4) == 0) {
                const char *cast_start = p + 1;
                /* Skip whitespace */
                while (*cast_start == ' ' || *cast_start == '\t') cast_start++;

                /* Check for void* cast */
                if (strncmp(cast_start, "void", 4) == 0) {
                    const char *q = cast_start + 4;
                    while (*q == ' ' || *q == '\t') q++;
                    if (*q == '*') {
                        /* Found (void*) cast */
                        q++;
                        while (*q == ' ' || *q == '\t') q++;
                        if (*q == ')') {
                            q++;
                            while (*q == ' ' || *q == '\t') q++;

                            /* Get identifier being cast */
                            if (isalpha(*q) || *q == '_') {
                                char id[128];
                                int i = 0;
                                while ((isalnum(*q) || *q == '_') && i < 127) {
                                    id[i++] = *q++;
                                }
                                id[i] = '\0';

                                /* Simple heuristic: if it's not a pointer deref or address-of, warn */
                                /* Check if previous char is & (address-of) */
                                const char *before = p - 1;
                                while (before > source && (*before == ' ' || *before == '\t')) before--;

                                if (*before != '&' && *before != '*') {
                                    add_issue(ctx, ISSUE_TYPE_SAFETY, SEVERITY_WARNING, line,
                                             "potential unsafe cast: (void*)%s", id);
                                }
                            }
                        }
                    }
                }
            }
        }
        p++;
    }
}

/* Check for NULL dereference issues */
static void check_null_deref(AnalysisContext *ctx, const char *source) {
    const char *p = source;
    int line = 1;
    int in_function = 0;
    int brace_depth = 0;

    while (*p) {
        if (*p == '\n') line++;

        if (*p == '{') {
            brace_depth++;
            if (brace_depth == 1) in_function = 1;
        } else if (*p == '}') {
            brace_depth--;
            if (brace_depth == 0) in_function = 0;
        }

        if (in_function && brace_depth > 0) {
            /* Look for -> operator (pointer dereference) */
            if (*p == '-' && *(p+1) == '>') {
                /* Backtrack to find pointer name */
                const char *q = p - 1;
                while (q > source && (*q == ' ' || *q == '\t')) q--;

                if (isalnum(*q) || *q == '_') {
                    const char *id_end = q + 1;
                    while (q > source && (isalnum(*(q-1)) || *(q-1) == '_')) q--;

                    char id[128];
                    int len = id_end - q;
                    if (len > 0 && len < 127) {
                        memcpy(id, q, len);
                        id[len] = '\0';

                        /* Simple heuristic: if no NULL check nearby, warn */
                        const char *check_start = (p > source + 200) ? p - 200 : source;
                        char check_buf[300];
                        int check_len = p - check_start;
                        if (check_len > 0 && check_len < 299) {
                            memcpy(check_buf, check_start, check_len);
                            check_buf[check_len] = '\0';

                            /* Look for if (ptr != NULL) or if (ptr) pattern */
                            char pattern1[140], pattern2[140];
                            snprintf(pattern1, sizeof(pattern1), "%s != NULL", id);
                            snprintf(pattern2, sizeof(pattern2), "if (%s)", id);

                            if (!strstr(check_buf, pattern1) && !strstr(check_buf, pattern2)) {
                                add_issue(ctx, ISSUE_NULL_DEREF, SEVERITY_WARNING, line,
                                         "potential NULL dereference of '%s' without NULL check", id);
                            }
                        }
                    }
                }
            }

            /* Look for * dereference */
            if (*p == '*' && (p == source || !isalnum(*(p-1)))) {
                const char *q = p + 1;
                while (*q == ' ' || *q == '\t') q++;

                if (isalpha(*q) || *q == '_') {
                    char id[128];
                    int i = 0;
                    while ((isalnum(*q) || *q == '_') && i < 127) {
                        id[i++] = *q++;
                    }
                    id[i] = '\0';

                    /* Check if it's a pointer variable */
                    Symbol *sym = find_symbol(ctx, id);
                    if (sym && !sym->is_function) {
                        /* Simple check: look for NULL check nearby */
                        const char *check_start = (p > source + 150) ? p - 150 : source;
                        int found_null_check = 0;
                        for (const char *s = check_start; s < p; s++) {
                            if (strncmp(s, "NULL", 4) == 0 || strncmp(s, "if (", 4) == 0) {
                                found_null_check = 1;
                                break;
                            }
                        }

                        if (!found_null_check) {
                            add_issue(ctx, ISSUE_NULL_DEREF, SEVERITY_INFO, line,
                                     "pointer '%s' dereferenced without visible NULL check", id);
                        }
                    }
                }
            }
        }
        p++;
    }
}

/* Check for unreachable code after return/exit */
static void check_unreachable_code(AnalysisContext *ctx, const char *source) {
    const char *p = source;
    int line = 1;
    int in_function = 0;
    int brace_depth = 0;
    int saved_brace_depth = 0;

    while (*p) {
        if (*p == '\n') line++;

        if (*p == '{') {
            brace_depth++;
            if (brace_depth == 1) in_function = 1;
        } else if (*p == '}') {
            brace_depth--;
            if (brace_depth == 0) in_function = 0;
        }

        if (in_function && brace_depth > 0) {
            /* Look for return, exit statements (not in if conditions) */
            if ((strncmp(p, "return", 6) == 0 && !isalnum(p[6]) && p[6] != '_') ||
                (strncmp(p, "exit", 4) == 0 && p[4] == '(')) {

                int stmt_line = line;
                saved_brace_depth = brace_depth;

                /* Skip to end of statement */
                while (*p && *p != ';') {
                    if (*p == '\n') line++;
                    p++;
                }
                if (*p == ';') p++;

                /* Skip whitespace and comments */
                while (*p) {
                    if (*p == ' ' || *p == '\t') {
                        p++;
                    } else if (*p == '\n') {
                        line++;
                        p++;
                    } else if (*p == '/' && *(p+1) == '/') {
                        while (*p && *p != '\n') p++;
                    } else if (*p == '/' && *(p+1) == '*') {
                        p += 2;
                        while (*p && !(*p == '*' && *(p+1) == '/')) {
                            if (*p == '\n') line++;
                            p++;
                        }
                        if (*p) p += 2;
                    } else {
                        break;
                    }
                }

                /* Only report unreachable if we're still at the same brace depth */
                /* This avoids false positives for "if (x) return;" patterns */
                if (brace_depth == saved_brace_depth && *p && *p != '}' && isalpha(*p)) {
                    /* Check it's not a label (followed by :) */
                    const char *q = p;
                    while (isalnum(*q) || *q == '_') q++;
                    while (*q == ' ' || *q == '\t') q++;

                    if (*q != ':') {
                        add_issue(ctx, ISSUE_UNREACHABLE_CODE, SEVERITY_WARNING, line,
                                 "unreachable code after return/exit at line %d", stmt_line);

                        /* Skip to end of current block to avoid duplicate warnings */
                        int target_depth = brace_depth - 1;
                        while (*p && brace_depth > target_depth) {
                            if (*p == '{') brace_depth++;
                            else if (*p == '}') brace_depth--;
                            if (*p == '\n') line++;
                            p++;
                        }
                        continue;
                    }
                }
            }
        }
        p++;
    }
}

/* Check for memory leaks (basic: malloc without free in function) */
static void check_memory_leaks(AnalysisContext *ctx, const char *source) {
    const char *p = source;
    int line = 1;
    int in_function = 0;
    int brace_depth = 0;
    int malloc_count = 0;
    int free_count = 0;
    int func_start_line = 0;

    while (*p) {
        if (*p == '\n') line++;

        if (*p == '{') {
            if (brace_depth == 0) {
                in_function = 1;
                func_start_line = line;
                malloc_count = 0;
                free_count = 0;
            }
            brace_depth++;
        } else if (*p == '}') {
            brace_depth--;
            if (brace_depth == 0 && in_function) {
                in_function = 0;

                /* Check for leaks - simple: more mallocs than frees */
                if (malloc_count > free_count && malloc_count > 0) {
                    add_issue(ctx, ISSUE_MEMORY_LEAK, SEVERITY_WARNING, func_start_line,
                             "potential memory leak: %d allocation(s) but only %d free(s)",
                             malloc_count, free_count);
                }
            }
        }

        if (in_function && brace_depth > 0) {
            /* Count malloc/calloc/realloc calls */
            if ((strncmp(p, "malloc", 6) == 0 && (p[6] == '(' || p[6] == ' ')) ||
                (strncmp(p, "calloc", 6) == 0 && (p[6] == '(' || p[6] == ' ')) ||
                (strncmp(p, "realloc", 7) == 0 && (p[7] == '(' || p[7] == ' '))) {
                malloc_count++;
            }

            /* Count free calls */
            if (strncmp(p, "free", 4) == 0 && (p[4] == '(' || p[4] == ' ')) {
                free_count++;
            }
        }
        p++;
    }
}

/* Check for unused local variables */
static void check_local_unused_vars(AnalysisContext *ctx, const char *source) {
    const char *p = source;
    int line = 1;
    int in_function = 0;
    int brace_depth = 0;

    typedef struct LocalVar {
        char name[128];
        int line;
        int used;
        struct LocalVar *next;
    } LocalVar;

    LocalVar *locals = NULL;

    while (*p) {
        if (*p == '\n') line++;

        if (*p == '{') {
            if (brace_depth == 0) {
                in_function = 1;
                /* Clear locals for new function */
                while (locals) {
                    LocalVar *next = locals->next;
                    free(locals);
                    locals = next;
                }
            }
            brace_depth++;
        } else if (*p == '}') {
            brace_depth--;
            if (brace_depth == 0 && in_function) {
                in_function = 0;

                /* Report unused locals */
                for (LocalVar *v = locals; v; v = v->next) {
                    if (!v->used && strcmp(v->name, "argc") != 0 &&
                        strcmp(v->name, "argv") != 0 && strncmp(v->name, "__", 2) != 0) {
                        add_issue(ctx, ISSUE_LOCAL_UNUSED, SEVERITY_WARNING, v->line,
                                 "unused local variable '%s'", v->name);
                    }
                }

                /* Clear locals */
                while (locals) {
                    LocalVar *next = locals->next;
                    free(locals);
                    locals = next;
                }
            }
        }

        if (in_function && brace_depth > 0) {
            /* Simple local variable detection: type identifier; */
            if (is_type_keyword_at(p)) {
                const char *type_start = p;

                /* Skip type keyword */
                while (*p && (isalnum(*p) || *p == '_')) p++;
                while (*p == ' ' || *p == '\t' || *p == '*') p++;

                /* Get identifier */
                if (isalpha(*p) || *p == '_') {
                    char id[128];
                    int i = 0;
                    const char *id_start = p;
                    int id_line = line;

                    while ((isalnum(*p) || *p == '_') && i < 127) {
                        id[i++] = *p++;
                    }
                    id[i] = '\0';

                    /* Check if followed by = or ; (variable declaration) */
                    while (*p == ' ' || *p == '\t') p++;

                    if (*p == '=' || *p == ';' || *p == '[') {
                        /* Add to locals list */
                        LocalVar *v = (LocalVar*)malloc(sizeof(LocalVar));
                        if (v) {
                            strncpy(v->name, id, sizeof(v->name) - 1);
                            v->name[sizeof(v->name) - 1] = '\0';
                            v->line = id_line;
                            v->used = 0;
                            v->next = locals;
                            locals = v;
                        }
                    }
                    continue;
                }
            }

            /* Check for usage of local vars */
            if (isalpha(*p) || *p == '_') {
                char id[128];
                int i = 0;
                while ((isalnum(*p) || *p == '_') && i < 127) {
                    id[i++] = *p++;
                }
                id[i] = '\0';

                /* Mark as used if found in locals */
                for (LocalVar *v = locals; v; v = v->next) {
                    if (strcmp(v->name, id) == 0) {
                        v->used = 1;
                        break;
                    }
                }
                continue;
            }
        }
        p++;
    }

    /* Cleanup remaining locals */
    while (locals) {
        LocalVar *next = locals->next;
        free(locals);
        locals = next;
    }
}

/* Helper: check if position starts with type keyword */
static int is_type_keyword_at(const char *p) {
    static const char *types[] = {
        "char", "short", "int", "long", "float", "double", "void",
        "signed", "unsigned", "struct", "union", "enum", NULL
    };

    for (int i = 0; types[i]; i++) {
        int len = strlen(types[i]);
        if (strncmp(p, types[i], len) == 0 && !isalnum(p[len]) && p[len] != '_') {
            return 1;
        }
    }
    return 0;
}

/* Check for uninitialized variables */
static void check_uninitialized_vars(AnalysisContext *ctx, const char *source) {
    const char *p = source;
    int line = 1;
    int in_function = 0;
    int brace_depth = 0;

    typedef struct InitVar {
        char name[128];
        int line;
        int initialized;
        struct InitVar *next;
    } InitVar;

    InitVar *vars = NULL;

    while (*p) {
        if (*p == '\n') line++;

        if (*p == '{') {
            if (brace_depth == 0) {
                in_function = 1;
                /* Clear for new function */
                while (vars) {
                    InitVar *next = vars->next;
                    free(vars);
                    vars = next;
                }
            }
            brace_depth++;
        } else if (*p == '}') {
            brace_depth--;
            if (brace_depth == 0 && in_function) {
                in_function = 0;

                /* Cleanup */
                while (vars) {
                    InitVar *next = vars->next;
                    free(vars);
                    vars = next;
                }
            }
        }

        if (in_function && brace_depth > 0) {
            /* Track variable declarations */
            if (is_type_keyword_at(p)) {
                /* Skip type */
                while (*p && (isalnum(*p) || *p == '_')) p++;
                while (*p == ' ' || *p == '\t' || *p == '*') p++;

                if (isalpha(*p) || *p == '_') {
                    char id[128];
                    int i = 0;
                    int id_line = line;

                    while ((isalnum(*p) || *p == '_') && i < 127) {
                        id[i++] = *p++;
                    }
                    id[i] = '\0';

                    while (*p == ' ' || *p == '\t') p++;

                    /* Check if initialized */
                    int initialized = (*p == '=');

                    if (*p == ';' || *p == '=' || *p == '[') {
                        /* Add to tracking */
                        InitVar *v = (InitVar*)malloc(sizeof(InitVar));
                        if (v) {
                            strncpy(v->name, id, sizeof(v->name) - 1);
                            v->name[sizeof(v->name) - 1] = '\0';
                            v->line = id_line;
                            v->initialized = initialized;
                            v->next = vars;
                            vars = v;
                        }
                    }
                    continue;
                }
            }

            /* Check for usage before initialization */
            if (isalpha(*p) || *p == '_') {
                char id[128];
                int i = 0;
                int usage_line = line;

                while ((isalnum(*p) || *p == '_') && i < 127) {
                    id[i++] = *p++;
                }
                id[i] = '\0';

                /* Check if it's being read (not assigned) */
                while (*p == ' ' || *p == '\t') p++;

                if (*p != '=' || *(p+1) == '=') { /* Not assignment (or comparison) */
                    for (InitVar *v = vars; v; v = v->next) {
                        if (strcmp(v->name, id) == 0 && !v->initialized) {
                            add_issue(ctx, ISSUE_UNINITIALIZED, SEVERITY_WARNING, usage_line,
                                     "variable '%s' may be used uninitialized (declared at line %d)",
                                     id, v->line);
                            v->initialized = 1; /* Mark to avoid duplicate warnings */
                            break;
                        }
                    }
                }

                /* If this is an assignment, mark as initialized */
                if (*p == '=' && *(p+1) != '=') {
                    for (InitVar *v = vars; v; v = v->next) {
                        if (strcmp(v->name, id) == 0) {
                            v->initialized = 1;
                            break;
                        }
                    }
                }
                continue;
            }
        }
        p++;
    }

    /* Cleanup */
    while (vars) {
        InitVar *next = vars->next;
        free(vars);
        vars = next;
    }
}

/* Analyze source file */
int analyze_file(const char *file, const AnalysisOptions *options, AnalysisResult *result) {
    FILE *fp = fopen(file, "r");
    if (!fp) {
        return ANALYZE_ERROR_FILE;
    }

    /* Read entire file */
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *source = (char*)malloc(size + 1);
    if (!source) {
        fclose(fp);
        return ANALYZE_ERROR_MEMORY;
    }

    size_t read_size = fread(source, 1, size, fp);
    source[read_size] = '\0';
    fclose(fp);

    /* Initialize analysis context */
    AnalysisContext ctx = {0};
    ctx.current_file = file;

    /* Pass 1: Find all declarations */
    if (options->check_dead_code || options->check_unused_vars) {
        analyze_pass1_declarations(&ctx, source);
    }

    /* Pass 2: Track usage */
    if (options->check_dead_code || options->check_unused_vars) {
        scan_usage(&ctx, source);
    }

    /* Debug: Print all symbols */
    if (options->verbose) {
        fprintf(stderr, "=== Symbol Table ===\n");
        for (Symbol *s = ctx.symbols; s; s = s->next) {
            fprintf(stderr, "%s (line %d): func=%d def=%d used=%d\n",
                   s->name, s->line, s->is_function, s->is_defined, s->is_used);
        }
        fprintf(stderr, "====================\n");
    }

    /* Pass 3: Report issues */
    if (options->check_dead_code) {
        for (Symbol *s = ctx.symbols; s; s = s->next) {
            if (s->is_function && s->is_defined && !s->is_used) {
                /* Allow main function */
                if (strcmp(s->name, "main") != 0) {
                    add_issue(&ctx, ISSUE_DEAD_CODE, SEVERITY_WARNING, s->line,
                             "unused function '%s'", s->name);
                }
            }
        }
    }

    if (options->check_unused_vars) {
        for (Symbol *s = ctx.symbols; s; s = s->next) {
            if (!s->is_function && s->is_declared && !s->is_used) {
                add_issue(&ctx, ISSUE_UNUSED_VAR, SEVERITY_WARNING, s->line,
                         "variable '%s' declared but never used", s->name);
            } else if (!s->is_function && s->read_count == 0 && s->write_count > 0) {
                add_issue(&ctx, ISSUE_UNUSED_VAR, SEVERITY_INFO, s->line,
                         "variable '%s' assigned but never read", s->name);
            }
        }
    }

    /* Type safety checks */
    if (options->check_type_safety) {
        check_type_safety(&ctx, source);
    }

    /* NULL dereference checks */
    if (options->check_null_deref) {
        check_null_deref(&ctx, source);
    }

    /* Unreachable code checks */
    if (options->check_unreachable) {
        check_unreachable_code(&ctx, source);
    }

    /* Memory leak checks */
    if (options->check_memory_leaks) {
        check_memory_leaks(&ctx, source);
    }

    /* Local unused variable checks */
    if (options->check_local_unused) {
        check_local_unused_vars(&ctx, source);
    }

    /* Uninitialized variable checks */
    if (options->check_uninitialized) {
        check_uninitialized_vars(&ctx, source);
    }

    /* Build result */
    result->total_issues = ctx.issue_count;
    result->error_count = 0;
    result->warning_count = 0;
    result->info_count = 0;
    result->issues = ctx.issues;

    /* Count by severity */
    for (AnalysisIssue *issue = ctx.issues; issue; issue = issue->next) {
        switch (issue->severity) {
            case SEVERITY_ERROR: result->error_count++; break;
            case SEVERITY_WARNING: result->warning_count++; break;
            case SEVERITY_INFO: result->info_count++; break;
        }
    }

    /* Cleanup */
    Symbol *s = ctx.symbols;
    while (s) {
        Symbol *next = s->next;
        free(s);
        s = next;
    }

    free(source);
    return 0;
}

/* Print analysis report */
void print_analysis_report(const AnalysisResult *result, const char *file) {
    printf("\n=== Static Analysis Report ===\n");
    printf("File: %s\n\n", file);

    if (result->total_issues == 0) {
        printf("No issues found.\n\n");
        return;
    }

    /* Group issues by type */
    int has_dead_code = 0, has_unused_vars = 0, has_type_safety = 0, has_null_deref = 0;
    int has_unreachable = 0, has_memory_leak = 0, has_local_unused = 0, has_uninitialized = 0;

    for (AnalysisIssue *issue = result->issues; issue; issue = issue->next) {
        switch (issue->type) {
            case ISSUE_DEAD_CODE: has_dead_code = 1; break;
            case ISSUE_UNUSED_VAR: has_unused_vars = 1; break;
            case ISSUE_TYPE_SAFETY: has_type_safety = 1; break;
            case ISSUE_NULL_DEREF: has_null_deref = 1; break;
            case ISSUE_UNREACHABLE_CODE: has_unreachable = 1; break;
            case ISSUE_MEMORY_LEAK: has_memory_leak = 1; break;
            case ISSUE_LOCAL_UNUSED: has_local_unused = 1; break;
            case ISSUE_UNINITIALIZED: has_uninitialized = 1; break;
            default: break;
        }
    }

    /* Print dead code issues */
    if (has_dead_code) {
        printf("Dead Code:\n");
        for (AnalysisIssue *issue = result->issues; issue; issue = issue->next) {
            if (issue->type == ISSUE_DEAD_CODE) {
                printf("  %s:%d: %s\n", issue->file, issue->line, issue->message);
            }
        }
        printf("\n");
    }

    /* Print unused variable issues */
    if (has_unused_vars) {
        printf("Unused Variables:\n");
        for (AnalysisIssue *issue = result->issues; issue; issue = issue->next) {
            if (issue->type == ISSUE_UNUSED_VAR) {
                printf("  %s:%d: %s\n", issue->file, issue->line, issue->message);
            }
        }
        printf("\n");
    }

    /* Print type safety issues */
    if (has_type_safety) {
        printf("Type Safety:\n");
        for (AnalysisIssue *issue = result->issues; issue; issue = issue->next) {
            if (issue->type == ISSUE_TYPE_SAFETY) {
                printf("  %s:%d: %s\n", issue->file, issue->line, issue->message);
            }
        }
        printf("\n");
    }

    /* Print NULL dereference issues */
    if (has_null_deref) {
        printf("Potential NULL Dereferences:\n");
        for (AnalysisIssue *issue = result->issues; issue; issue = issue->next) {
            if (issue->type == ISSUE_NULL_DEREF) {
                printf("  %s:%d: %s\n", issue->file, issue->line, issue->message);
            }
        }
        printf("\n");
    }

    /* Print unreachable code issues */
    if (has_unreachable) {
        printf("Unreachable Code:\n");
        for (AnalysisIssue *issue = result->issues; issue; issue = issue->next) {
            if (issue->type == ISSUE_UNREACHABLE_CODE) {
                printf("  %s:%d: %s\n", issue->file, issue->line, issue->message);
            }
        }
        printf("\n");
    }

    /* Print memory leak issues */
    if (has_memory_leak) {
        printf("Memory Leaks:\n");
        for (AnalysisIssue *issue = result->issues; issue; issue = issue->next) {
            if (issue->type == ISSUE_MEMORY_LEAK) {
                printf("  %s:%d: %s\n", issue->file, issue->line, issue->message);
            }
        }
        printf("\n");
    }

    /* Print local unused variable issues */
    if (has_local_unused) {
        printf("Unused Local Variables:\n");
        for (AnalysisIssue *issue = result->issues; issue; issue = issue->next) {
            if (issue->type == ISSUE_LOCAL_UNUSED) {
                printf("  %s:%d: %s\n", issue->file, issue->line, issue->message);
            }
        }
        printf("\n");
    }

    /* Print uninitialized variable issues */
    if (has_uninitialized) {
        printf("Uninitialized Variables:\n");
        for (AnalysisIssue *issue = result->issues; issue; issue = issue->next) {
            if (issue->type == ISSUE_UNINITIALIZED) {
                printf("  %s:%d: %s\n", issue->file, issue->line, issue->message);
            }
        }
        printf("\n");
    }

    printf("Summary: %d issue%s found ", result->total_issues,
           result->total_issues == 1 ? "" : "s");
    printf("(%d error%s, %d warning%s, %d info)\n\n",
           result->error_count, result->error_count == 1 ? "" : "s",
           result->warning_count, result->warning_count == 1 ? "" : "s",
           result->info_count);
}

/* Free analysis result */
void free_analysis_result(AnalysisResult *result) {
    AnalysisIssue *issue = result->issues;
    while (issue) {
        AnalysisIssue *next = issue->next;
        free(issue);
        issue = next;
    }
    result->issues = NULL;
    result->total_issues = 0;
}
