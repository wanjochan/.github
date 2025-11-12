/**
 * @file cosmo_mutate.c
 * @brief Mutation Testing Implementation
 */

#include "cosmo_mutate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_MUTANTS 1000
#define MAX_SOURCE_SIZE (1024 * 1024)  /* 1MB max source file */

/* Mutation site location in source */
typedef struct {
    int line;
    int column;
    int offset;          /* Byte offset in source */
    int length;          /* Length of original text */
    mutation_op_t op;
    char original[128];
    char mutated[128];
} mutation_site_t;

/* Mutator context */
struct mutator_t {
    char *source_file;
    char *source_content;
    size_t source_length;
    mutant_t *mutants;
    int mutant_count;
    int mutant_capacity;
    mutation_site_t *sites;
    int site_count;
    int site_capacity;
};

/* Operator mutation table */
typedef struct {
    const char *from;
    const char *to;
} op_mutation_t;

static const op_mutation_t binary_ops[] = {
    {"+", "-"}, {"-", "+"}, {"*", "/"}, {"/", "*"}, {"%", "*"},
    {"==", "!="}, {"!=", "=="}, {"<", "<="}, {">", ">="},
    {"<=", "<"}, {">=", ">"},
    {"&&", "||"}, {"||", "&&"},
    {"|", "&"}, {"&", "|"}, {"^", "&"},
    {"<<", ">>"}, {">>", "<<"},
    {NULL, NULL}
};

/* Forward declarations */
static int parse_and_find_mutations(mutator_t *mut, int ops);
static int find_operator_mutations(mutator_t *mut, const char *src, size_t len);
static int find_constant_mutations(mutator_t *mut, const char *src, size_t len);
static int find_condition_mutations(mutator_t *mut, const char *src, size_t len);
static int find_return_mutations(mutator_t *mut, const char *src, size_t len);
static void add_mutation_site(mutator_t *mut, int line, int col, int offset,
                              int length, mutation_op_t op,
                              const char *orig, const char *mutated);
static void skip_whitespace(const char **p);
static void skip_comment(const char **p);
static int is_identifier_char(char c);
static int get_line_number(const char *src, const char *pos);
static int get_column_number(const char *src, const char *pos);

/* Create mutator */
mutator_t* mutator_create(const char *source_file) {
    if (!source_file) return NULL;

    mutator_t *mut = (mutator_t*)calloc(1, sizeof(mutator_t));
    if (!mut) return NULL;

    mut->source_file = strdup(source_file);

    /* Read source file */
    FILE *fp = fopen(source_file, "rb");
    if (!fp) {
        free(mut->source_file);
        free(mut);
        return NULL;
    }

    fseek(fp, 0, SEEK_END);
    long file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    if (file_size <= 0 || file_size > MAX_SOURCE_SIZE) {
        fclose(fp);
        free(mut->source_file);
        free(mut);
        return NULL;
    }

    mut->source_content = (char*)malloc(file_size + 1);
    if (!mut->source_content) {
        fclose(fp);
        free(mut->source_file);
        free(mut);
        return NULL;
    }

    size_t read_size = fread(mut->source_content, 1, file_size, fp);
    fclose(fp);

    mut->source_content[read_size] = '\0';
    mut->source_length = read_size;

    /* Allocate mutant arrays */
    mut->mutant_capacity = 100;
    mut->mutants = (mutant_t*)calloc(mut->mutant_capacity, sizeof(mutant_t));

    mut->site_capacity = 100;
    mut->sites = (mutation_site_t*)calloc(mut->site_capacity, sizeof(mutation_site_t));

    return mut;
}

/* Generate mutants */
int mutator_generate_mutants(mutator_t *mut, int ops, int max_mutants) {
    if (!mut) return -1;

    /* Parse source and find mutation sites */
    if (parse_and_find_mutations(mut, ops) < 0) {
        return -1;
    }

    /* Limit mutants if requested */
    int total = mut->site_count;
    if (max_mutants > 0 && total > max_mutants) {
        total = max_mutants;
    }

    /* Convert sites to mutants */
    for (int i = 0; i < total; i++) {
        if (mut->mutant_count >= mut->mutant_capacity) {
            mut->mutant_capacity *= 2;
            mut->mutants = (mutant_t*)realloc(mut->mutants,
                                             mut->mutant_capacity * sizeof(mutant_t));
        }

        mutant_t *m = &mut->mutants[mut->mutant_count++];
        mutation_site_t *s = &mut->sites[i];

        m->file = mut->source_file;
        m->line = s->line;
        m->column = s->column;
        m->op = s->op;
        strncpy(m->original, s->original, sizeof(m->original) - 1);
        strncpy(m->mutated, s->mutated, sizeof(m->mutated) - 1);
        m->status = MUTANT_PENDING;
        m->error_msg[0] = '\0';
    }

    return mut->mutant_count;
}

/* Apply mutant to source */
int mutator_apply_mutant(mutator_t *mut, int mutant_id, const char *output_file) {
    if (!mut || mutant_id < 0 || mutant_id >= mut->mutant_count) {
        return -1;
    }

    mutation_site_t *site = &mut->sites[mutant_id];

    /* Create mutated source */
    FILE *fp = fopen(output_file, "w");
    if (!fp) return -1;

    /* Write before mutation */
    fwrite(mut->source_content, 1, site->offset, fp);

    /* Write mutated code */
    fputs(site->mutated, fp);

    /* Write after mutation */
    const char *after = mut->source_content + site->offset + site->length;
    fputs(after, fp);

    fclose(fp);
    return 0;
}

/* Test mutant */
int mutator_test_mutant(mutator_t *mut, int mutant_id, const char *test_cmd) {
    if (!mut || mutant_id < 0 || mutant_id >= mut->mutant_count) {
        return -1;
    }

    mutant_t *m = &mut->mutants[mutant_id];

    /* Apply mutation to temp file */
    char temp_file[256];
    snprintf(temp_file, sizeof(temp_file), "/tmp/mutant_%d.c", getpid());

    if (mutator_apply_mutant(mut, mutant_id, temp_file) < 0) {
        m->status = MUTANT_ERROR;
        snprintf(m->error_msg, sizeof(m->error_msg), "Failed to apply mutation");
        return -1;
    }

    /* Compile mutated source */
    char compile_cmd[512];
    snprintf(compile_cmd, sizeof(compile_cmd),
             "gcc -o /tmp/mutant_%d.out %s 2>/dev/null", getpid(), temp_file);

    int compile_result = system(compile_cmd);

    if (compile_result != 0) {
        m->status = MUTANT_ERROR;
        snprintf(m->error_msg, sizeof(m->error_msg), "Compilation failed");
        unlink(temp_file);
        return -1;
    }

    /* Run test command */
    int test_result = 0;
    if (test_cmd && test_cmd[0]) {
        test_result = system(test_cmd);
    } else {
        /* Default: just run the compiled program */
        char run_cmd[256];
        snprintf(run_cmd, sizeof(run_cmd), "/tmp/mutant_%d.out 2>/dev/null", getpid());
        test_result = system(run_cmd);
    }

    /* Mutant is killed if test fails (non-zero exit) */
    if (test_result != 0) {
        m->status = MUTANT_KILLED;
    } else {
        m->status = MUTANT_SURVIVED;
    }

    /* Cleanup */
    unlink(temp_file);
    char out_file[256];
    snprintf(out_file, sizeof(out_file), "/tmp/mutant_%d.out", getpid());
    unlink(out_file);

    return (m->status == MUTANT_KILLED) ? 0 : 1;
}

/* Get mutant info */
const mutant_t* mutator_get_mutant(mutator_t *mut, int mutant_id) {
    if (!mut || mutant_id < 0 || mutant_id >= mut->mutant_count) {
        return NULL;
    }
    return &mut->mutants[mutant_id];
}

/* Get count */
int mutator_get_count(mutator_t *mut) {
    return mut ? mut->mutant_count : 0;
}

/* Calculate mutation score */
double mutator_get_score(mutator_t *mut) {
    if (!mut || mut->mutant_count == 0) return 0.0;

    int killed = 0;
    int total = 0;

    for (int i = 0; i < mut->mutant_count; i++) {
        if (mut->mutants[i].status == MUTANT_KILLED) {
            killed++;
        }
        if (mut->mutants[i].status != MUTANT_PENDING) {
            total++;
        }
    }

    return total > 0 ? (killed * 100.0 / total) : 0.0;
}

/* Print report */
void mutator_print_report(mutator_t *mut, FILE *fp) {
    if (!mut || !fp) return;

    fprintf(fp, "=== Mutation Testing Report ===\n");
    fprintf(fp, "Source: %s\n", mut->source_file);
    fprintf(fp, "Total Mutants: %d\n", mut->mutant_count);

    int killed = 0, survived = 0, error = 0, pending = 0;

    for (int i = 0; i < mut->mutant_count; i++) {
        switch (mut->mutants[i].status) {
            case MUTANT_KILLED: killed++; break;
            case MUTANT_SURVIVED: survived++; break;
            case MUTANT_ERROR: error++; break;
            case MUTANT_PENDING: pending++; break;
        }
    }

    fprintf(fp, "Killed: %d (tests detected the bug)\n", killed);
    fprintf(fp, "Survived: %d (tests MISSED the bug)\n", survived);
    fprintf(fp, "Errors: %d (compilation/runtime errors)\n", error);
    fprintf(fp, "Pending: %d (not yet tested)\n", pending);

    double score = mutator_get_score(mut);
    fprintf(fp, "Mutation Score: %.1f%% (%d/%d)\n", score, killed,
            killed + survived);

    fprintf(fp, "\nSurviving Mutants:\n");
    int count = 1;
    for (int i = 0; i < mut->mutant_count; i++) {
        if (mut->mutants[i].status == MUTANT_SURVIVED) {
            mutant_t *m = &mut->mutants[i];
            fprintf(fp, "%d. %s:%d - ", count++, m->file, m->line);

            switch (m->op) {
                case MUT_FLIP_OPERATOR:
                    fprintf(fp, "Operator flip: %s → %s", m->original, m->mutated);
                    break;
                case MUT_CHANGE_CONSTANT:
                    fprintf(fp, "Constant: %s → %s", m->original, m->mutated);
                    break;
                case MUT_NEGATE_CONDITION:
                    fprintf(fp, "Negate: %s → %s", m->original, m->mutated);
                    break;
                case MUT_REPLACE_RETURN:
                    fprintf(fp, "Return: %s → %s", m->original, m->mutated);
                    break;
                default:
                    fprintf(fp, "%s → %s", m->original, m->mutated);
            }
            fprintf(fp, " (SURVIVED)\n");
        }
    }

    if (survived == 0) {
        fprintf(fp, "(none)\n");
    }

    fprintf(fp, "\nRecommendation: ");
    if (score >= 90.0) {
        fprintf(fp, "Excellent test coverage!\n");
    } else if (score >= 80.0) {
        fprintf(fp, "Good test coverage. Consider adding tests for surviving mutants.\n");
    } else if (score >= 70.0) {
        fprintf(fp, "Moderate test coverage. Add more tests to improve quality.\n");
    } else {
        fprintf(fp, "Poor test coverage. Many bugs are not being caught by tests!\n");
    }
}

/* Destroy mutator */
void mutator_destroy(mutator_t *mut) {
    if (!mut) return;

    free(mut->source_file);
    free(mut->source_content);
    free(mut->mutants);
    free(mut->sites);
    free(mut);
}

/* === Helper Functions === */

static void add_mutation_site(mutator_t *mut, int line, int col, int offset,
                              int length, mutation_op_t op,
                              const char *orig, const char *mutated) {
    if (mut->site_count >= mut->site_capacity) {
        mut->site_capacity *= 2;
        mut->sites = (mutation_site_t*)realloc(mut->sites,
                                              mut->site_capacity * sizeof(mutation_site_t));
    }

    mutation_site_t *s = &mut->sites[mut->site_count++];
    s->line = line;
    s->column = col;
    s->offset = offset;
    s->length = length;
    s->op = op;
    strncpy(s->original, orig, sizeof(s->original) - 1);
    strncpy(s->mutated, mutated, sizeof(s->mutated) - 1);
}

static void skip_whitespace(const char **p) {
    while (**p && isspace(**p)) (*p)++;
}

static void skip_comment(const char **p) {
    if ((*p)[0] == '/' && (*p)[1] == '/') {
        while (**p && **p != '\n') (*p)++;
    } else if ((*p)[0] == '/' && (*p)[1] == '*') {
        (*p) += 2;
        while (**p && !((*p)[0] == '*' && (*p)[1] == '/')) (*p)++;
        if (**p) (*p) += 2;
    }
}

static int is_identifier_char(char c) {
    return isalnum(c) || c == '_';
}

static int get_line_number(const char *src, const char *pos) {
    int line = 1;
    for (const char *p = src; p < pos; p++) {
        if (*p == '\n') line++;
    }
    return line;
}

static int get_column_number(const char *src, const char *pos) {
    int col = 1;
    for (const char *p = pos; p > src && *(p-1) != '\n'; p--) {
        col++;
    }
    return col;
}

/* Parse and find mutations */
static int parse_and_find_mutations(mutator_t *mut, int ops) {
    const char *src = mut->source_content;
    size_t len = mut->source_length;

    mut->site_count = 0;

    if (ops & (1 << MUT_FLIP_OPERATOR) || ops == MUT_ALL) {
        find_operator_mutations(mut, src, len);
    }

    if (ops & (1 << MUT_CHANGE_CONSTANT) || ops == MUT_ALL) {
        find_constant_mutations(mut, src, len);
    }

    if (ops & (1 << MUT_NEGATE_CONDITION) || ops == MUT_ALL) {
        find_condition_mutations(mut, src, len);
    }

    if (ops & (1 << MUT_REPLACE_RETURN) || ops == MUT_ALL) {
        find_return_mutations(mut, src, len);
    }

    return mut->site_count;
}

/* Find operator mutations */
static int find_operator_mutations(mutator_t *mut, const char *src, size_t len) {
    const char *p = src;
    const char *end = src + len;

    while (p < end) {
        skip_whitespace(&p);
        if (p >= end) break;

        /* Skip comments and strings */
        if (*p == '/' && (p[1] == '/' || p[1] == '*')) {
            skip_comment(&p);
            continue;
        }
        if (*p == '"') {
            p++;
            while (p < end && *p != '"') {
                if (*p == '\\') p++;
                p++;
            }
            if (p < end) p++;
            continue;
        }
        if (*p == '\'') {
            p++;
            if (p < end && *p == '\\') p++;
            if (p < end) p++;
            if (p < end && *p == '\'') p++;
            continue;
        }

        /* Check for binary operators */
        for (int i = 0; binary_ops[i].from; i++) {
            const char *op_from = binary_ops[i].from;
            const char *op_to = binary_ops[i].to;
            size_t op_len = strlen(op_from);

            if (p + op_len <= end && strncmp(p, op_from, op_len) == 0) {
                /* Make sure it's not part of a longer operator */
                if (p + op_len < end && strchr("=<>&|+-*/", p[op_len])) {
                    p++;
                    continue;
                }

                int line = get_line_number(src, p);
                int col = get_column_number(src, p);
                int offset = p - src;

                add_mutation_site(mut, line, col, offset, op_len,
                                 MUT_FLIP_OPERATOR, op_from, op_to);

                p += op_len;
                break;
            }
        }

        p++;
    }

    return 0;
}

/* Find constant mutations */
static int find_constant_mutations(mutator_t *mut, const char *src, size_t len) {
    const char *p = src;
    const char *end = src + len;

    while (p < end) {
        skip_whitespace(&p);
        if (p >= end) break;

        /* Skip comments and strings */
        if (*p == '/' && (p[1] == '/' || p[1] == '*')) {
            skip_comment(&p);
            continue;
        }
        if (*p == '"' || *p == '\'') {
            p++;
            continue;
        }

        /* Check for numeric constants */
        if (isdigit(*p)) {
            const char *start = p;
            long val = 0;

            /* Parse integer */
            while (p < end && isdigit(*p)) {
                val = val * 10 + (*p - '0');
                p++;
            }

            /* Skip if followed by identifier char (part of variable name) */
            if (p < end && is_identifier_char(*p)) {
                continue;
            }

            /* Skip floats for now */
            if (p < end && (*p == '.' || *p == 'e' || *p == 'E')) {
                while (p < end && (isdigit(*p) || *p == '.' || *p == 'e' ||
                                  *p == 'E' || *p == '+' || *p == '-')) {
                    p++;
                }
                continue;
            }

            /* Generate mutation */
            char orig[32], mutated[32];
            int len_num = p - start;
            snprintf(orig, sizeof(orig), "%.*s", len_num, start);

            if (val == 0) {
                strcpy(mutated, "1");
            } else if (val == 1) {
                strcpy(mutated, "0");
            } else {
                snprintf(mutated, sizeof(mutated), "%ld", val + 1);
            }

            int line = get_line_number(src, start);
            int col = get_column_number(src, start);
            int offset = start - src;

            add_mutation_site(mut, line, col, offset, len_num,
                             MUT_CHANGE_CONSTANT, orig, mutated);
            continue;
        }

        p++;
    }

    return 0;
}

/* Find condition mutations */
static int find_condition_mutations(mutator_t *mut, const char *src, size_t len) {
    const char *p = src;
    const char *end = src + len;

    while (p < end) {
        skip_whitespace(&p);
        if (p >= end) break;

        /* Look for if/while statements */
        if (strncmp(p, "if", 2) == 0 || strncmp(p, "while", 5) == 0) {
            int kw_len = (*p == 'i') ? 2 : 5;

            /* Make sure it's a keyword, not part of identifier */
            if (p + kw_len < end && is_identifier_char(p[kw_len])) {
                p++;
                continue;
            }

            p += kw_len;
            skip_whitespace(&p);

            if (p < end && *p == '(') {
                p++;
                const char *cond_start = p;

                /* Find matching closing paren */
                int paren_count = 1;
                while (p < end && paren_count > 0) {
                    if (*p == '(') paren_count++;
                    else if (*p == ')') paren_count--;
                    if (paren_count > 0) p++;
                }

                if (paren_count == 0 && p > cond_start) {
                    /* Found condition */
                    int cond_len = p - cond_start;
                    char orig[128], mutated[128];

                    snprintf(orig, sizeof(orig), "%.*s", cond_len, cond_start);
                    snprintf(mutated, sizeof(mutated), "!(%.*s)", cond_len, cond_start);

                    int line = get_line_number(src, cond_start);
                    int col = get_column_number(src, cond_start);
                    int offset = cond_start - src;

                    add_mutation_site(mut, line, col, offset, cond_len,
                                     MUT_NEGATE_CONDITION, orig, mutated);
                }

                continue;
            }
        }

        p++;
    }

    return 0;
}

/* Find return mutations */
static int find_return_mutations(mutator_t *mut, const char *src, size_t len) {
    const char *p = src;
    const char *end = src + len;

    while (p < end) {
        skip_whitespace(&p);
        if (p >= end) break;

        /* Look for return statements */
        if (strncmp(p, "return", 6) == 0) {
            /* Make sure it's a keyword */
            if (p + 6 < end && is_identifier_char(p[6])) {
                p++;
                continue;
            }

            const char *ret_start = p;
            p += 6;
            skip_whitespace(&p);

            /* Find the expression */
            const char *expr_start = p;

            /* Parse until semicolon */
            while (p < end && *p != ';') {
                p++;
            }

            if (p > expr_start && *p == ';') {
                int expr_len = p - expr_start;

                /* Skip empty returns */
                const char *tmp = expr_start;
                skip_whitespace(&tmp);
                if (tmp >= p) {
                    p++;
                    continue;
                }

                char orig[128], mutated[128];
                snprintf(orig, sizeof(orig), "return %.*s", expr_len, expr_start);
                strcpy(mutated, "return 0");

                int line = get_line_number(src, ret_start);
                int col = get_column_number(src, ret_start);
                int offset = ret_start - src;
                int total_len = p - ret_start;

                add_mutation_site(mut, line, col, offset, total_len,
                                 MUT_REPLACE_RETURN, orig, mutated);
            }

            continue;
        }

        p++;
    }

    return 0;
}
