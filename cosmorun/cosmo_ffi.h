/* FFI Auto-Generator for CosmoRun
 * Automatically generates Foreign Function Interface bindings from C headers
 */

#ifndef COSMO_FFI_H
#define COSMO_FFI_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Maximum limits for parsing */
#define FFI_MAX_LINE_LENGTH     4096
#define FFI_MAX_PARAMS          32
#define FFI_MAX_FUNCTIONS       1024
#define FFI_MAX_STRUCTS         256
#define FFI_MAX_ENUMS           128
#define FFI_MAX_TYPEDEFS        256
#define FFI_MAX_NAME_LENGTH     256
#define FFI_MAX_TYPE_LENGTH     512

/* FFI generator options */
typedef struct {
    const char *input_header;       // Input header file path
    const char *output_file;        // Output bindings file path
    const char *library_name;       // Shared library name (e.g., "libm.so")
    int verbose;                    // Verbose output flag
    int generate_loader;            // Generate loader function
    int add_error_checks;           // Add error checking code
} ffi_options_t;

/* C type categories */
typedef enum {
    FFI_TYPE_VOID,
    FFI_TYPE_CHAR,
    FFI_TYPE_SHORT,
    FFI_TYPE_INT,
    FFI_TYPE_LONG,
    FFI_TYPE_LONG_LONG,
    FFI_TYPE_FLOAT,
    FFI_TYPE_DOUBLE,
    FFI_TYPE_POINTER,
    FFI_TYPE_STRUCT,
    FFI_TYPE_ENUM,
    FFI_TYPE_FUNCTION_PTR,
    FFI_TYPE_UNKNOWN
} ffi_type_category_t;

/* Parameter information */
typedef struct {
    char name[FFI_MAX_NAME_LENGTH];
    char type[FFI_MAX_TYPE_LENGTH];
    ffi_type_category_t category;
    int is_pointer;
    int is_const;
} ffi_param_t;

/* Function declaration information */
typedef struct {
    char name[FFI_MAX_NAME_LENGTH];
    char return_type[FFI_MAX_TYPE_LENGTH];
    ffi_type_category_t return_category;
    int return_is_pointer;
    ffi_param_t params[FFI_MAX_PARAMS];
    int param_count;
    int is_variadic;
} ffi_function_t;

/* Struct member information */
typedef struct {
    char name[FFI_MAX_NAME_LENGTH];
    char type[FFI_MAX_TYPE_LENGTH];
} ffi_struct_member_t;

/* Struct definition */
typedef struct {
    char name[FFI_MAX_NAME_LENGTH];
    ffi_struct_member_t members[FFI_MAX_PARAMS];
    int member_count;
} ffi_struct_t;

/* Enum value */
typedef struct {
    char name[FFI_MAX_NAME_LENGTH];
    int value;
    int has_value;
} ffi_enum_value_t;

/* Enum definition */
typedef struct {
    char name[FFI_MAX_NAME_LENGTH];
    ffi_enum_value_t values[FFI_MAX_PARAMS];
    int value_count;
} ffi_enum_t;

/* Typedef definition */
typedef struct {
    char alias[FFI_MAX_NAME_LENGTH];
    char original[FFI_MAX_TYPE_LENGTH];
} ffi_typedef_t;

/* FFI generator context */
typedef struct {
    ffi_options_t options;
    ffi_function_t functions[FFI_MAX_FUNCTIONS];
    int function_count;
    ffi_struct_t structs[FFI_MAX_STRUCTS];
    int struct_count;
    ffi_enum_t enums[FFI_MAX_ENUMS];
    int enum_count;
    ffi_typedef_t typedefs[FFI_MAX_TYPEDEFS];
    int typedef_count;
} ffi_context_t;

/* Core FFI generator functions */

/* Initialize FFI context */
ffi_context_t *ffi_context_create(const ffi_options_t *options);

/* Destroy FFI context */
void ffi_context_destroy(ffi_context_t *ctx);

/* Parse C header file */
int ffi_parse_header(ffi_context_t *ctx, const char *header_path);

/* Generate bindings code */
int ffi_generate_bindings(ffi_context_t *ctx, const char *output_path);

/* Helper functions */

/* Parse a function declaration line */
int ffi_parse_function_declaration(const char *line, ffi_function_t *func);

/* Parse struct definition */
int ffi_parse_struct(const char *text, ffi_struct_t *s);

/* Parse enum definition */
int ffi_parse_enum(const char *text, ffi_enum_t *e);

/* Parse typedef */
int ffi_parse_typedef(const char *line, ffi_typedef_t *td);

/* Determine type category from type string */
ffi_type_category_t ffi_get_type_category(const char *type_str);

/* Generate function pointer declaration */
void ffi_generate_function_pointer(FILE *out, const ffi_function_t *func);

/* Generate dlsym loading code */
void ffi_generate_loader_code(FILE *out, const ffi_context_t *ctx);

/* Utility functions */

/* Trim whitespace from string */
char *ffi_trim_whitespace(char *str);

/* Check if line is a comment or empty */
int ffi_is_comment_or_empty(const char *line);

/* Extract function signature from preprocessor noise */
int ffi_extract_function_signature(const char *line, char *signature, size_t size);

/* Remove preprocessor directives from line */
void ffi_remove_preprocessor(char *line);

#endif /* COSMO_FFI_H */
