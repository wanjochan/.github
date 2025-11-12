/*
 * CosmoRun Dynamic Loader Interface
 * Public API for dlopen/dlsym/dlclose/dlerror
 */

#ifndef COSMO_DL_H
#define COSMO_DL_H

#ifdef __cplusplus
extern "C" {
#endif

/* RTLD flags for dlopen - use system definitions if available */
#ifndef RTLD_LAZY
#define RTLD_LAZY     0x00001  // Lazy function call binding
#endif
#ifndef RTLD_NOW
#define RTLD_NOW      0x00002  // Immediate function call binding
#endif
#ifndef RTLD_BINDING_MASK
#define RTLD_BINDING_MASK   0x3
#endif
#ifndef RTLD_NOLOAD
#define RTLD_NOLOAD   0x00004  // Don't load, just check if already loaded
#endif
#ifndef RTLD_DEEPBIND
#define RTLD_DEEPBIND 0x00008  // Place lookup scope ahead of global scope
#endif
#ifndef RTLD_GLOBAL
#define RTLD_GLOBAL   0x00100  // Symbols available for subsequently loaded libs
#endif
#ifndef RTLD_LOCAL
#define RTLD_LOCAL    0x00000  // Symbols not available (default)
#endif
#ifndef RTLD_NODELETE
#define RTLD_NODELETE 0x01000  // Don't unload on dlclose
#endif
#ifndef RTLD_NODEFAULTLIB
#define RTLD_NODEFAULTLIB 0x02000  // Don't search default libraries
#endif

/* Special handles for dlsym */
#ifndef RTLD_DEFAULT
#define RTLD_DEFAULT  ((void*)0)  // Find first occurrence in search order
#endif
#ifndef RTLD_NEXT
#define RTLD_NEXT     ((void*)-1L) // Find next occurrence after current object
#endif

/*
 * dlopen - Load dynamic shared object
 * @filename: Path to shared library, NULL for main program
 * @flags: RTLD_* flags
 * Returns: Handle to loaded library, or NULL on error
 */
void *cosmo_dlopen(const char *filename, int flags);

/*
 * dlsym - Get address of symbol in shared object
 * @handle: Library handle from dlopen, or RTLD_DEFAULT/RTLD_NEXT
 * @symbol: Symbol name
 * Returns: Address of symbol, or NULL on error
 */
void *cosmo_dlsym(void *handle, const char *symbol);

/*
 * dlclose - Close dynamic shared object
 * @handle: Library handle from dlopen
 * Returns: 0 on success, -1 on error
 */
int cosmo_dlclose(void *handle);

/*
 * dlerror - Get human-readable error string
 * Returns: Error string, or NULL if no error
 * Note: Thread-local, cleared on next dl* call
 */
char *cosmo_dlerror(void);

/*
 * dladdr - Get information about address (extension)
 * @addr: Address to query
 * @info: Output structure
 * Returns: 0 on error, non-zero on success
 */
typedef struct {
    const char *dli_fname;  /* Pathname of shared object */
    void       *dli_fbase;  /* Base address of shared object */
    const char *dli_sname;  /* Name of nearest symbol */
    void       *dli_saddr;  /* Address of nearest symbol */
} Dl_info;

int cosmo_dladdr(const void *addr, Dl_info *info);

#ifdef __cplusplus
}
#endif

#endif /* COSMO_DL_H */
