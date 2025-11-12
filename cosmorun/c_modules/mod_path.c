#include "mod_path.h"
#include <ctype.h>
#include <string.h>

/* Helper: safe strndup replacement */
static char* safe_strndup(const char* s, size_t n) {
    size_t len = shim_strlen(s);
    if (len > n) len = n;
    char* result = shim_malloc(len + 1);
    if (result) {
        shim_memcpy(result, s, len);
        result[len] = '\0';
    }
    return result;
}

/* Platform detection - use Cosmopolitan's runtime detection
 * Cosmopolitan provides IsWindows(), IsLinux(), IsXnu() etc. */
#ifdef __COSMORUN__
/* Cosmopolitan provides these runtime detection functions */
extern const int __hostos;
#define _HOSTWINDOWS 4
#define IS_WINDOWS ((__hostos & _HOSTWINDOWS) != 0)
#else
/* Fallback for non-Cosmopolitan builds */
#ifdef _WIN32
#define IS_WINDOWS 1
#else
#define IS_WINDOWS 0
#endif
#endif

/* Helper: check if character is a separator */
static int is_sep(char c) {
    return c == '/' || (IS_WINDOWS && c == '\\');
}

/* Helper: get preferred separator */
char path_sep(void) {
    return IS_WINDOWS ? '\\' : '/';
}

/* Helper: get PATH delimiter */
char path_delimiter(void) {
    return IS_WINDOWS ? ';' : ':';
}

/* Helper: check if Windows drive letter (e.g., "C:") */
static int is_windows_drive(const char* path) {
    if (!IS_WINDOWS) return 0;
    return path && isalpha(path[0]) && path[1] == ':';
}

/* Check if path is absolute */
int path_is_absolute(const char* path) {
    if (!path || !*path) return 0;

    /* Unix: starts with / */
    if (path[0] == '/') return 1;

    /* Windows: C:\ or C:/ or \\ (UNC) */
    if (IS_WINDOWS) {
        if (is_windows_drive(path) && is_sep(path[2])) return 1;
        if (path[0] == '\\' && path[1] == '\\') return 1;
    }

    return 0;
}

/* Join exactly two paths */
char* path_join2(const char* path1, const char* path2) {
    if (!path1 || !*path1) return path2 ? shim_strdup(path2) : shim_strdup("");
    if (!path2 || !*path2) return shim_strdup(path1);

    size_t len1 = shim_strlen(path1);
    size_t len2 = shim_strlen(path2);
    int need_sep = !is_sep(path1[len1 - 1]) && !is_sep(path2[0]);

    char* result = shim_malloc(len1 + len2 + (need_sep ? 2 : 1));
    if (!result) return NULL;

    shim_strcpy(result, path1);
    if (need_sep) {
        result[len1] = path_sep();
        result[len1 + 1] = '\0';
    }
    shim_strcat(result, path2);

    return result;
}

/* Join 3 paths */
char* path_join3(const char* p1, const char* p2, const char* p3) {
    char* temp = path_join2(p1, p2);
    if (!temp) return NULL;
    char* result = path_join2(temp, p3);
    shim_free(temp);
    return result;
}

/* Join 4 paths */
char* path_join4(const char* p1, const char* p2, const char* p3, const char* p4) {
    char* temp1 = path_join3(p1, p2, p3);
    if (!temp1) return NULL;
    char* result = path_join2(temp1, p4);
    shim_free(temp1);
    return result;
}

/* Get directory name */
char* path_dirname(const char* path) {
    if (!path || !*path) return shim_strdup(".");

    char* copy = shim_strdup(path);
    if (!copy) return NULL;

    /* Remove trailing separators */
    size_t len = shim_strlen(copy);
    while (len > 1 && is_sep(copy[len - 1])) {
        copy[--len] = '\0';
    }

    /* Find last separator */
    char* last_sep = NULL;
    for (char* p = copy + len - 1; p >= copy; p--) {
        if (is_sep(*p)) {
            last_sep = p;
            break;
        }
    }

    if (!last_sep) {
        /* No separator found */
        shim_free(copy);
        return shim_strdup(".");
    }

    /* Handle root path */
    if (last_sep == copy) {
        copy[1] = '\0';
        return copy;
    }

    /* Handle Windows drive (C:) */
    if (IS_WINDOWS && last_sep == copy + 2 && is_windows_drive(copy)) {
        copy[3] = '\0';
        return copy;
    }

    *last_sep = '\0';
    return copy;
}

/* Get base name */
char* path_basename(const char* path) {
    if (!path || !*path) return shim_strdup("");

    /* Special case for root */
    if (shim_strcmp(path, "/") == 0) {
        return shim_strdup("/");
    }

    /* Remove trailing separators */
    char* copy = shim_strdup(path);
    if (!copy) return NULL;

    size_t len = shim_strlen(copy);
    while (len > 1 && is_sep(copy[len - 1])) {
        copy[--len] = '\0';
    }

    /* Find last separator */
    char* last_sep = NULL;
    for (char* p = copy + len - 1; p >= copy; p--) {
        if (is_sep(*p)) {
            last_sep = p;
            break;
        }
    }

    char* result;
    if (last_sep) {
        result = shim_strdup(last_sep + 1);
    } else {
        result = shim_strdup(copy);
    }

    shim_free(copy);
    return result;
}

/* Get extension */
char* path_extname(const char* path) {
    if (!path || !*path) return shim_strdup("");

    char* base = path_basename(path);
    if (!base) return shim_strdup("");

    /* Find last dot */
    char* last_dot = shim_strrchr(base, '.');

    /* No dot, or dot is first character (hidden file) */
    if (!last_dot || last_dot == base) {
        shim_free(base);
        return shim_strdup("");
    }

    char* result = shim_strdup(last_dot);
    shim_free(base);
    return result;
}

/* Normalize path */
char* path_normalize(const char* path) {
    if (!path || !*path) return shim_strdup(".");

    /* Detect if absolute */
    int is_abs = path_is_absolute(path);
    char sep = path_sep();

    /* Split into components */
    char** parts = shim_malloc(sizeof(char*) * (shim_strlen(path) + 1));
    if (!parts) return NULL;

    int part_count = 0;
    char* copy = shim_strdup(path);
    char* token = strtok(copy, "/\\");

    while (token) {
        if (shim_strcmp(token, ".") == 0) {
            /* Skip current directory */
        } else if (shim_strcmp(token, "..") == 0) {
            /* Go up if possible */
            if (part_count > 0 && shim_strcmp(parts[part_count - 1], "..") != 0) {
                shim_free(parts[--part_count]);
            } else if (!is_abs) {
                parts[part_count++] = shim_strdup("..");
            }
        } else {
            parts[part_count++] = shim_strdup(token);
        }
        token = strtok(NULL, "/\\");
    }
    shim_free(copy);

    /* Build result */
    size_t total_len = 0;
    for (int i = 0; i < part_count; i++) {
        total_len += shim_strlen(parts[i]) + 1;
    }

    char* result = shim_malloc(total_len + (is_abs ? 2 : 1));
    if (!result) {
        for (int i = 0; i < part_count; i++) shim_free(parts[i]);
        shim_free(parts);
        return NULL;
    }

    result[0] = '\0';
    if (is_abs) {
        result[0] = sep;
        result[1] = '\0';
    }

    for (int i = 0; i < part_count; i++) {
        if (i > 0) {
            size_t curr_len = shim_strlen(result);
            result[curr_len] = sep;
            result[curr_len + 1] = '\0';
        }
        shim_strcat(result, parts[i]);
        shim_free(parts[i]);
    }
    shim_free(parts);

    if (result[0] == '\0') {
        shim_strcpy(result, ".");
    }

    return result;
}

/* Resolve to absolute path */
char* path_resolve(const char* path) {
    if (!path || !*path) {
        return getcwd(NULL, 0);
    }

    if (path_is_absolute(path)) {
        return path_normalize(path);
    }

    char* cwd = getcwd(NULL, 0);
    if (!cwd) return NULL;

    char* joined = path_join2(cwd, path);
    shim_free(cwd);

    if (!joined) return NULL;

    char* normalized = path_normalize(joined);
    shim_free(joined);

    return normalized;
}

/* Parse path into components */
path_parse_t* path_parse(const char* path) {
    if (!path) return NULL;

    path_parse_t* result = shim_calloc(1, sizeof(path_parse_t));
    if (!result) return NULL;

    /* Extract root */
    if (path[0] == '/') {
        result->root = shim_strdup("/");
    } else if (IS_WINDOWS && is_windows_drive(path)) {
        result->root = safe_strndup(path, 3);  /* e.g., "C:\" */
    } else {
        result->root = shim_strdup("");
    }

    /* Get directory and base */
    result->dir = path_dirname(path);
    result->base = path_basename(path);

    /* Extract name and extension */
    char* dot = shim_strrchr(result->base, '.');
    if (dot && dot != result->base) {
        result->ext = shim_strdup(dot);
        result->name = safe_strndup(result->base, dot - result->base);
    } else {
        result->ext = shim_strdup("");
        result->name = shim_strdup(result->base);
    }

    return result;
}

/* Free parsed path */
void path_parse_free(path_parse_t* parsed) {
    if (!parsed) return;
    shim_free(parsed->root);
    shim_free(parsed->dir);
    shim_free(parsed->base);
    shim_free(parsed->name);
    shim_free(parsed->ext);
    shim_free(parsed);
}
