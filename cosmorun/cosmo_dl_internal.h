/*
 * CosmoRun Dynamic Loader - Internal Structures
 * Internal data structures for dynamic loading implementation
 */

#ifndef COSMO_DL_INTERNAL_H
#define COSMO_DL_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of loaded libraries */
#define DL_MAX_HANDLES 256

/* Maximum error message length */
#define DL_ERROR_MAX_LEN 512

/* Library handle structure */
typedef struct dl_handle {
    void *native_handle;      /* Native OS handle (dlopen, LoadLibrary, etc) */
    char *filename;           /* Library filename/path */
    int flags;                /* RTLD_* flags */
    int refcount;             /* Reference counter */
    struct dl_handle *next;   /* Linked list */
    void *base_addr;          /* Base address of loaded library */
    void **init_array;        /* Array of constructor functions */
    size_t init_array_size;   /* Number of constructors */
    void **fini_array;        /* Array of destructor functions */
    size_t fini_array_size;   /* Number of destructors */
} dl_handle_t;

/* Global state for dynamic loader */
typedef struct {
    dl_handle_t *handles;     /* Linked list of loaded libraries */
    int handle_count;         /* Number of loaded libraries */
    void *main_handle;        /* Handle to main executable */
    int initialized;          /* Initialization flag */
} dl_state_t;

/* Error storage - static for now to avoid TLS issues in JIT */
/* These are accessed via functions, not directly */

/* Global state */
extern dl_state_t g_dl_state;

/* Internal functions */
void dl_set_error(const char *fmt, ...);
void dl_clear_error(void);
dl_handle_t *dl_find_handle(void *handle);
dl_handle_t *dl_create_handle(const char *filename, int flags);
void dl_destroy_handle(dl_handle_t *h);
int dl_init_state(void);
void dl_cleanup_state(void);

/* Platform-specific native wrappers */
void *dl_native_open(const char *filename, int flags);
void *dl_native_sym(void *handle, const char *symbol);
int dl_native_close(void *handle);
const char *dl_native_error(void);

#ifdef __cplusplus
}
#endif

#endif /* COSMO_DL_INTERNAL_H */
