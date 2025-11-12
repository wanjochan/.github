#ifndef MOD_FFI_H
#define MOD_FFI_H

#include "src/cosmo_libc.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Type codes compatible with libffi */
#define FFI_TYPE_VOID     0
#define FFI_TYPE_INT      1
#define FFI_TYPE_FLOAT    2
#define FFI_TYPE_DOUBLE   3
#define FFI_TYPE_POINTER  4
#define FFI_TYPE_SINT8    5
#define FFI_TYPE_UINT8    6
#define FFI_TYPE_SINT16   7
#define FFI_TYPE_UINT16   8
#define FFI_TYPE_SINT32   9
#define FFI_TYPE_UINT32   10
#define FFI_TYPE_SINT64   11
#define FFI_TYPE_UINT64   12

typedef struct ffi_type ffi_type;

struct ffi_type {
    size_t size;
    unsigned short alignment;
    unsigned short type;
    struct ffi_type **elements;
};

typedef enum ffi_status {
    FFI_OK = 0,
    FFI_BAD_TYPEDEF = 1,
    FFI_BAD_ABI = 2,
} ffi_status;

typedef enum ffi_abi {
    FFI_FIRST_ABI = 0,
    FFI_SYSV = 0,
    FFI_DEFAULT_ABI = FFI_SYSV,
    FFI_LAST_ABI = FFI_SYSV
} ffi_abi;

typedef struct ffi_cif {
    ffi_abi abi;
    unsigned nargs;
    ffi_type **arg_types;
    ffi_type *rtype;
    unsigned bytes;
    unsigned flags;
    unsigned short nfixedargs;
    unsigned char int_count;
    unsigned char float_count;
} ffi_cif;

extern ffi_type ffi_type_void;
extern ffi_type ffi_type_sint8;
extern ffi_type ffi_type_uint8;
extern ffi_type ffi_type_sint16;
extern ffi_type ffi_type_uint16;
extern ffi_type ffi_type_sint32;
extern ffi_type ffi_type_uint32;
extern ffi_type ffi_type_sint64;
extern ffi_type ffi_type_uint64;
extern ffi_type ffi_type_pointer;
extern ffi_type ffi_type_float;
extern ffi_type ffi_type_double;

ffi_status ffi_prep_cif(ffi_cif *cif, ffi_abi abi, unsigned nargs,
                        ffi_type *rtype, ffi_type **arg_types);
void ffi_call(const ffi_cif *cif, void (*fn)(void), void *rvalue, void **avalue);

#ifdef __cplusplus
}
#endif

#endif /* MOD_FFI_H */
