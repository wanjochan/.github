#ifndef _STDDEF_H_
#define _STDDEF_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Standard type definitions */
#ifndef NULL
#define NULL ((void*)0)
#endif

#ifndef _SIZE_T_DEFINED
#define _SIZE_T_DEFINED
typedef unsigned long size_t;
#endif

#ifndef _PTRDIFF_T_DEFINED
#define _PTRDIFF_T_DEFINED
typedef long ptrdiff_t;
#endif

#ifndef _WCHAR_T_DEFINED
#define _WCHAR_T_DEFINED
typedef int wchar_t;
#endif

/* Offset of member in structure */
#define offsetof(type, member) ((size_t)&((type*)0)->member)

#ifdef __cplusplus
}
#endif

#endif /* _STDDEF_H_ */