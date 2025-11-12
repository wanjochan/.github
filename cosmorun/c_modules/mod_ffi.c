#include "src/cosmo_libc.h"
#include "mod_ffi.h"

#if defined(__x86_64__) || defined(__amd64__)
#define FFI_PLATFORM_X86_64 1
#elif defined(__aarch64__) || defined(__arm64__)
#define FFI_PLATFORM_AARCH64 1
#endif

/* Global ffi_type instances mirroring libffi layout */
#define DEFINE_TYPE(name, tcode, tsize)                    \
    ffi_type name = {                                      \
        .size = (tsize),                                   \
        .alignment = (unsigned short)((tsize) ? (tsize) : 1), \
        .type = (tcode),                                   \
        .elements = NULL                                   \
    }

DEFINE_TYPE(ffi_type_void,     FFI_TYPE_VOID,    0);
DEFINE_TYPE(ffi_type_sint8,    FFI_TYPE_SINT8,   sizeof(int8_t));
DEFINE_TYPE(ffi_type_uint8,    FFI_TYPE_UINT8,   sizeof(uint8_t));
DEFINE_TYPE(ffi_type_sint16,   FFI_TYPE_SINT16,  sizeof(int16_t));
DEFINE_TYPE(ffi_type_uint16,   FFI_TYPE_UINT16,  sizeof(uint16_t));
DEFINE_TYPE(ffi_type_sint32,   FFI_TYPE_SINT32,  sizeof(int32_t));
DEFINE_TYPE(ffi_type_uint32,   FFI_TYPE_UINT32,  sizeof(uint32_t));
DEFINE_TYPE(ffi_type_sint64,   FFI_TYPE_SINT64,  sizeof(int64_t));
DEFINE_TYPE(ffi_type_uint64,   FFI_TYPE_UINT64,  sizeof(uint64_t));
DEFINE_TYPE(ffi_type_pointer,  FFI_TYPE_POINTER, sizeof(void *));
DEFINE_TYPE(ffi_type_float,    FFI_TYPE_FLOAT,   sizeof(float));
DEFINE_TYPE(ffi_type_double,   FFI_TYPE_DOUBLE,  sizeof(double));

#undef DEFINE_TYPE

enum arg_class {
    ARG_CLASS_NONE = 0,
    ARG_CLASS_INTEGER = 1,
    ARG_CLASS_FLOAT = 2,
};

static enum arg_class classify_type(const ffi_type *type) {
    switch (type->type) {
        case FFI_TYPE_SINT8:
        case FFI_TYPE_UINT8:
        case FFI_TYPE_SINT16:
        case FFI_TYPE_UINT16:
        case FFI_TYPE_SINT32:
        case FFI_TYPE_UINT32:
        case FFI_TYPE_SINT64:
        case FFI_TYPE_UINT64:
        case FFI_TYPE_POINTER:
            return ARG_CLASS_INTEGER;
        case FFI_TYPE_FLOAT:
        case FFI_TYPE_DOUBLE:
            return ARG_CLASS_FLOAT;
        case FFI_TYPE_VOID:
            return ARG_CLASS_NONE;
        default:
            return -1;
    }
}

ffi_status ffi_prep_cif(ffi_cif *cif, ffi_abi abi, unsigned nargs,
                        ffi_type *rtype, ffi_type **arg_types) {
    if (!cif || !rtype || (nargs && !arg_types)) {
        return FFI_BAD_TYPEDEF;
    }

    if (abi != FFI_SYSV) {
        return FFI_BAD_ABI;
    }

    unsigned int_count = 0;

    for (unsigned i = 0; i < nargs; ++i) {
        const ffi_type *ty = arg_types[i];
        if (!ty) return FFI_BAD_TYPEDEF;

        enum arg_class cls = classify_type(ty);
        if ((int)cls < 0) {
            return FFI_BAD_TYPEDEF;
        }
        if (cls == ARG_CLASS_INTEGER) {
            ++int_count;
        } else if (cls == ARG_CLASS_FLOAT) {
            return FFI_BAD_TYPEDEF;
        }
    }

#if !defined(FFI_PLATFORM_X86_64) && !defined(FFI_PLATFORM_AARCH64)
    (void)int_count;
    return FFI_BAD_ABI;
#else
    #ifdef FFI_PLATFORM_X86_64
    if (int_count > 6) {
        return FFI_BAD_TYPEDEF;
    }
    #elif defined(FFI_PLATFORM_AARCH64)
    if (int_count > 8) {
        return FFI_BAD_TYPEDEF;
    }
    #endif

    enum arg_class ret_class = classify_type(rtype);
    if ((int)ret_class < 0 && rtype->type != FFI_TYPE_VOID) {
        return FFI_BAD_TYPEDEF;
    }

    cif->abi = abi;
    cif->nargs = nargs;
    cif->arg_types = arg_types;
    cif->rtype = rtype;
    cif->bytes = 0;
    cif->flags = (unsigned)ret_class;
    cif->nfixedargs = nargs;
    cif->int_count = (unsigned char)int_count;
    cif->float_count = 0;

    return FFI_OK;
#endif
}

#if defined(FFI_PLATFORM_X86_64) || defined(FFI_PLATFORM_AARCH64)

static uint64_t load_int_arg(const ffi_type *type, void *ptr) {
    switch (type->type) {
        case FFI_TYPE_SINT8:
            return (uint64_t)(int64_t)*(int8_t *)ptr;
        case FFI_TYPE_UINT8:
            return (uint64_t)*(uint8_t *)ptr;
        case FFI_TYPE_SINT16:
            return (uint64_t)(int64_t)*(int16_t *)ptr;
        case FFI_TYPE_UINT16:
            return (uint64_t)*(uint16_t *)ptr;
        case FFI_TYPE_SINT32:
            return (uint64_t)(int64_t)*(int32_t *)ptr;
        case FFI_TYPE_UINT32:
            return (uint64_t)*(uint32_t *)ptr;
        case FFI_TYPE_SINT64:
        case FFI_TYPE_UINT64:
            return *(uint64_t *)ptr;
        case FFI_TYPE_POINTER:
            return (uint64_t)(uintptr_t)*(void **)ptr;
        default:
            return 0;
    }
}

#endif /* FFI_PLATFORM_X86_64 || FFI_PLATFORM_AARCH64 */

void ffi_call(const ffi_cif *cif, void (*fn)(void), void *rvalue, void **avalue) {
#ifdef FFI_PLATFORM_X86_64
    if (!cif || !fn) {
        return;
    }
    if (cif->abi != FFI_SYSV) {
        return;
    }
    if (cif->nargs && !avalue) {
        return;
    }

    uint64_t int_regs[6] = {0};
    unsigned int_index = 0;

    for (unsigned i = 0; i < cif->nargs; ++i) {
        const ffi_type *ty = cif->arg_types[i];
        enum arg_class cls = classify_type(ty);

        if (cls == ARG_CLASS_INTEGER) {
            if (int_index < 6) {
                int_regs[int_index++] = load_int_arg(ty, avalue[i]);
            } else {
                /* Unsupported spill path */
                return;
            }
        } else {
            return;
        }
    }

    /* Prepare r8/r9 separately because inline asm constraints lack direct specifiers */
    if (int_index > 4) {
        __asm__ volatile("mov %0, %%r8" : : "r"(int_regs[4]));
    } else {
        uint64_t zero = 0;
        __asm__ volatile("mov %0, %%r8" : : "r"(zero));
    }
    if (int_index > 5) {
        __asm__ volatile("mov %0, %%r9" : : "r"(int_regs[5]));
    } else {
        uint64_t zero = 0;
        __asm__ volatile("mov %0, %%r9" : : "r"(zero));
    }

    uint64_t a0 = (int_index > 0) ? int_regs[0] : 0;
    uint64_t a1 = (int_index > 1) ? int_regs[1] : 0;
    uint64_t a2 = (int_index > 2) ? int_regs[2] : 0;
    uint64_t a3 = (int_index > 3) ? int_regs[3] : 0;

    uint64_t ret_int = 0;

    __asm__ volatile(
        "call *%[fn]\n"
        : "=a"(ret_int)
        : [fn]"r"(fn),
          "D"(a0),
          "S"(a1),
          "d"(a2),
          "c"(a3)
        : "memory");

    if (!rvalue && cif->rtype->type != FFI_TYPE_VOID) {
        return;
    }

    switch (cif->rtype->type) {
        case FFI_TYPE_VOID:
            break;
        case FFI_TYPE_SINT8:
            *(int8_t *)rvalue = (int8_t)ret_int;
            break;
        case FFI_TYPE_UINT8:
            *(uint8_t *)rvalue = (uint8_t)ret_int;
            break;
        case FFI_TYPE_SINT16:
            *(int16_t *)rvalue = (int16_t)ret_int;
            break;
        case FFI_TYPE_UINT16:
            *(uint16_t *)rvalue = (uint16_t)ret_int;
            break;
        case FFI_TYPE_SINT32:
            *(int32_t *)rvalue = (int32_t)ret_int;
            break;
        case FFI_TYPE_UINT32:
            *(uint32_t *)rvalue = (uint32_t)ret_int;
            break;
        case FFI_TYPE_SINT64:
        case FFI_TYPE_UINT64:
            *(uint64_t *)rvalue = ret_int;
            break;
        case FFI_TYPE_POINTER:
            *(void **)rvalue = (void *)(uintptr_t)ret_int;
            break;
        default:
            break;
    }
#elif defined(FFI_PLATFORM_AARCH64)
    if (!cif || !fn) {
        return;
    }
    if (cif->abi != FFI_SYSV) {
        return;
    }
    if (cif->nargs && !avalue) {
        return;
    }

    uint64_t int_regs[8] = {0};
    unsigned int_index = 0;

    for (unsigned i = 0; i < cif->nargs; ++i) {
        const ffi_type *ty = cif->arg_types[i];
        enum arg_class cls = classify_type(ty);

        if (cls == ARG_CLASS_INTEGER) {
            if (int_index < 8) {
                int_regs[int_index++] = load_int_arg(ty, avalue[i]);
            } else {
                /* Unsupported spill path */
                return;
            }
        } else {
            return;
        }
    }

    /* ARM64: Use C function pointer call instead of inline asm */
    typedef uint64_t (*fn_ptr_t)(uint64_t, uint64_t, uint64_t, uint64_t,
                                  uint64_t, uint64_t, uint64_t, uint64_t);
    fn_ptr_t fn_ptr = (fn_ptr_t)fn;
    uint64_t ret_int = fn_ptr(int_regs[0], int_regs[1], int_regs[2], int_regs[3],
                               int_regs[4], int_regs[5], int_regs[6], int_regs[7]);

    if (!rvalue && cif->rtype->type != FFI_TYPE_VOID) {
        return;
    }

    switch (cif->rtype->type) {
        case FFI_TYPE_VOID:
            break;
        case FFI_TYPE_SINT8:
            *(int8_t *)rvalue = (int8_t)ret_int;
            break;
        case FFI_TYPE_UINT8:
            *(uint8_t *)rvalue = (uint8_t)ret_int;
            break;
        case FFI_TYPE_SINT16:
            *(int16_t *)rvalue = (int16_t)ret_int;
            break;
        case FFI_TYPE_UINT16:
            *(uint16_t *)rvalue = (uint16_t)ret_int;
            break;
        case FFI_TYPE_SINT32:
            *(int32_t *)rvalue = (int32_t)ret_int;
            break;
        case FFI_TYPE_UINT32:
            *(uint32_t *)rvalue = (uint32_t)ret_int;
            break;
        case FFI_TYPE_SINT64:
        case FFI_TYPE_UINT64:
            *(uint64_t *)rvalue = ret_int;
            break;
        case FFI_TYPE_POINTER:
            *(void **)rvalue = (void *)(uintptr_t)ret_int;
            break;
        default:
            break;
    }
#else
    (void)cif;
    (void)fn;
    (void)rvalue;
    (void)avalue;
#endif /* FFI_PLATFORM_X86_64 || FFI_PLATFORM_AARCH64 */
}
