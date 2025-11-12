#ifndef COSMORUN_PATH_H
#define COSMORUN_PATH_H

/*
 * mod_path - Node.js-style path manipulation utilities
 *
 * Cross-platform path handling for Unix (/) and Windows (\) separators
 * All returned strings are caller-owned (must be freed after use)
 */

#include "../cosmorun_system/libc_shim/sys_libc_shim.h"

/* ==================== Path Components ==================== */

/* Parse result structure */
typedef struct {
    char* root;     /* root part (e.g., "/" or "C:\") */
    char* dir;      /* directory part */
    char* base;     /* filename with extension */
    char* name;     /* filename without extension */
    char* ext;      /* extension with dot */
} path_parse_t;

/* ==================== Core Functions ==================== */

/* Join exactly two paths */
char* path_join2(const char* path1, const char* path2);

/* Join 3 paths */
char* path_join3(const char* p1, const char* p2, const char* p3);

/* Join 4 paths */
char* path_join4(const char* p1, const char* p2, const char* p3, const char* p4);

/* Get directory name (removes last component) */
char* path_dirname(const char* path);

/* Get file name (last component) */
char* path_basename(const char* path);

/* Get file extension (including dot, e.g., ".txt") */
char* path_extname(const char* path);

/* Normalize path (resolve .., ., remove redundant separators) */
char* path_normalize(const char* path);

/* Resolve to absolute path */
char* path_resolve(const char* path);

/* ==================== Path Properties ==================== */

/* Check if path is absolute */
int path_is_absolute(const char* path);

/* Get path separator for current platform */
char path_sep(void);

/* Get delimiter for PATH environment variable */
char path_delimiter(void);

/* ==================== Path Parsing ==================== */

/* Parse path into components */
path_parse_t* path_parse(const char* path);

/* Free parsed path structure */
void path_parse_free(path_parse_t* parsed);

#endif /* COSMORUN_PATH_H */
