#ifndef COSMORUN_XDL_H
#define COSMORUN_XDL_H

#ifdef __cplusplus
extern "C" {
#endif

typedef void* xdl_handle;

xdl_handle xdl_open(const char* filename, int flags);
void*      xdl_sym(xdl_handle handle, const char* symbol);
int        xdl_close(xdl_handle handle);
const char* xdl_error(void);

#ifdef __cplusplus
}
#endif

#endif /* COSMORUN_XDL_H */
