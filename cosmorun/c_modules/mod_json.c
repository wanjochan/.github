// mod_json.c - JSON parsing and serialization for cosmorun
// 提供 cJSON 嵌入式库封装
//

// 首先包含 cosmorun libc 接口以避免重定义冲突
#include "src/cosmo_libc.h"
#include "mod_json.h"

/* Include error handling implementation (header-only) */
#include "mod_error_impl.h"

/* ==================== Embedded cJSON Library ==================== */

// Define constants that cJSON needs
#ifndef DBL_EPSILON
#define DBL_EPSILON 2.2204460492503131e-16
#endif
#ifndef NAN
#define NAN (0.0/0.0)
#endif
#ifndef INFINITY
#define INFINITY (1.0/0.0)
#endif

// Provide math functions that cJSON needs
static inline int __builtin_isnan(double x) {
    return x != x;
}

static inline int __builtin_isinf(double x) {
    return __builtin_isnan(x - x) && !__builtin_isnan(x);
}

// Also provide standard names
#ifndef isnan
#define isnan(x) __builtin_isnan(x)
#endif
#ifndef isinf
#define isinf(x) __builtin_isinf(x)
#endif

// Include cJSON library directly - no dynamic loading needed
#define CJSON_HIDE_SYMBOLS 1

// Prevent standard headers from being included by cJSON and redefining types
// cosmo_libc.h has already defined FILE, int64_t, and other standard types
#define _STDIO_H 1
#define _BITS_TYPES_FILE_H 1
#define _STDLIB_H 1
#define _BITS_STDINT_INTN_H 1
#define _BITS_STDINT_UINTN_H 1
#define _MATH_H 1
#define _FLOAT_H 1
#define _CTYPE_H 1
#define _LIMITS_H 1

// Define DBL_EPSILON and related constants
#ifndef __DBL_EPSILON__
#define __DBL_EPSILON__ 2.2204460492503131e-16
#endif
#ifndef DBL_EPSILON
#define DBL_EPSILON __DBL_EPSILON__
#endif

#include "third_party/cJSON.h"
#include "third_party/cJSON.c"

/* ==================== Internal cJSON Structures ==================== */

// Use cJSON's native structure (already defined in cJSON.h)
typedef cJSON cJSON_internal;

/* ==================== Context Management ==================== */

json_context_t* json_init(const char *lib_path) {
    // lib_path 参数被忽略 - 使用嵌入式 cJSON
    (void)lib_path;

    // 使用 System Layer API 分配内存
    json_context_t *ctx = (json_context_t*)shim_malloc(sizeof(json_context_t));
    if (!ctx) {
        COSMORUN_ERROR(COSMORUN_ERR_OUT_OF_MEMORY, "json_init: Out of memory");
        return NULL;
    }

    // Set lib_handle to non-NULL to indicate initialization success
    ctx->lib_handle = (void*)0x1;

    // Link function pointers to embedded cJSON functions
    ctx->parse_fn = (void* (*)(const char*))cJSON_Parse;
    ctx->parse_with_length_fn = (void* (*)(const char*, size_t))cJSON_ParseWithLength;
    ctx->print_fn = (char* (*)(const void*))cJSON_Print;
    ctx->print_unformatted_fn = (char* (*)(const void*))cJSON_PrintUnformatted;
    ctx->delete_fn = (void (*)(void*))cJSON_Delete;
    ctx->free_fn = (void (*)(void*))cJSON_free;

    // Object operations
    ctx->create_object_fn = (void* (*)(void))cJSON_CreateObject;
    ctx->get_object_item_fn = (void* (*)(const void*, const char*))cJSON_GetObjectItem;
    ctx->has_object_item_fn = (int (*)(const void*, const char*))cJSON_HasObjectItem;
    ctx->add_item_to_object_fn = (int (*)(void*, const char*, void*))cJSON_AddItemToObject;
    ctx->detach_item_from_object_fn = (void* (*)(void*, const char*))cJSON_DetachItemFromObject;
    ctx->delete_item_from_object_fn = (void (*)(void*, const char*))cJSON_DeleteItemFromObject;

    // Array operations
    ctx->create_array_fn = (void* (*)(void))cJSON_CreateArray;
    ctx->get_array_size_fn = (int (*)(const void*))cJSON_GetArraySize;
    ctx->get_array_item_fn = (void* (*)(const void*, int))cJSON_GetArrayItem;
    ctx->add_item_to_array_fn = (int (*)(void*, void*))cJSON_AddItemToArray;
    ctx->detach_item_from_array_fn = (void* (*)(void*, int))cJSON_DetachItemFromArray;
    ctx->delete_item_from_array_fn = (void (*)(void*, int))cJSON_DeleteItemFromArray;

    // Value creation
    ctx->create_null_fn = (void* (*)(void))cJSON_CreateNull;
    ctx->create_true_fn = (void* (*)(void))cJSON_CreateTrue;
    ctx->create_false_fn = (void* (*)(void))cJSON_CreateFalse;
    ctx->create_bool_fn = (void* (*)(int))cJSON_CreateBool;
    ctx->create_number_fn = (void* (*)(double))cJSON_CreateNumber;
    ctx->create_string_fn = (void* (*)(const char*))cJSON_CreateString;

    // Value inspection
    ctx->is_null_fn = (int (*)(const void*))cJSON_IsNull;
    ctx->is_false_fn = (int (*)(const void*))cJSON_IsFalse;
    ctx->is_true_fn = (int (*)(const void*))cJSON_IsTrue;
    ctx->is_bool_fn = (int (*)(const void*))cJSON_IsBool;
    ctx->is_number_fn = (int (*)(const void*))cJSON_IsNumber;
    ctx->is_string_fn = (int (*)(const void*))cJSON_IsString;
    ctx->is_array_fn = (int (*)(const void*))cJSON_IsArray;
    ctx->is_object_fn = (int (*)(const void*))cJSON_IsObject;

    return ctx;
}

void json_cleanup(json_context_t *ctx) {
    if (!ctx) return;

    // cJSON 是静态嵌入的，不需要 dlclose
    ctx->lib_handle = NULL;

    // 使用 System Layer API 释放内存
    shim_free(ctx);
}

/* ==================== Parsing & Serialization ==================== */

json_value_t* json_parse(json_context_t *ctx, const char *str) {
    if (!ctx || !ctx->parse_fn || !str) {
        COSMORUN_ERROR(COSMORUN_ERR_NULL_POINTER, "json_parse: ctx or str is NULL");
        return NULL;
    }
    json_value_t* result = (json_value_t*)ctx->parse_fn(str);
    if (!result) {
        COSMORUN_ERROR(COSMORUN_ERR_PARSE_FAILED, "json_parse: Failed to parse JSON string");
    }
    return result;
}

json_value_t* json_parse_length(json_context_t *ctx, const char *str, size_t length) {
    if (!ctx || !str) return NULL;
    if (ctx->parse_with_length_fn) {
        return (json_value_t*)ctx->parse_with_length_fn(str, length);
    }
    // Fallback to regular parse
    return json_parse(ctx, str);
}

char* json_stringify(json_context_t *ctx, json_value_t *val) {
    if (!ctx || !ctx->print_fn || !val) return NULL;
    return ctx->print_fn((void*)val);
}

char* json_stringify_compact(json_context_t *ctx, json_value_t *val) {
    if (!ctx || !val) return NULL;
    if (ctx->print_unformatted_fn) {
        return ctx->print_unformatted_fn((void*)val);
    }
    // Fallback to regular print
    return json_stringify(ctx, val);
}

void json_free_string(json_context_t *ctx, char *str) {
    if (ctx && str) {
        if (ctx->free_fn) {
            ctx->free_fn(str);
        } else {
            // 使用 System Layer API 释放字符串
            shim_free(str);
        }
    }
}

void json_free(json_context_t *ctx, json_value_t *val) {
    if (ctx && ctx->delete_fn && val) {
        ctx->delete_fn((void*)val);
    }
}

/* ==================== Value Type Inspection ==================== */

json_type_t json_type(json_context_t *ctx, json_value_t *val) {
    if (!ctx || !val) return JSON_TYPE_NULL;

    cJSON_internal *item = (cJSON_internal*)val;
    int type = item->type;

    if (type & cJSON_NULL) return JSON_TYPE_NULL;
    if (type & cJSON_False) return JSON_TYPE_FALSE;
    if (type & cJSON_True) return JSON_TYPE_TRUE;
    if (type & cJSON_Number) return JSON_TYPE_NUMBER;
    if (type & cJSON_String) return JSON_TYPE_STRING;
    if (type & cJSON_Array) return JSON_TYPE_ARRAY;
    if (type & cJSON_Object) return JSON_TYPE_OBJECT;

    return JSON_TYPE_NULL;
}

int json_is_null(json_context_t *ctx, json_value_t *val) {
    if (!ctx || !val) return 0;
    if (ctx->is_null_fn) {
        return ctx->is_null_fn((void*)val);
    }
    return json_type(ctx, val) == JSON_TYPE_NULL;
}

int json_is_bool(json_context_t *ctx, json_value_t *val) {
    if (!ctx || !val) return 0;
    if (ctx->is_bool_fn) {
        return ctx->is_bool_fn((void*)val);
    }
    json_type_t t = json_type(ctx, val);
    return t == JSON_TYPE_TRUE || t == JSON_TYPE_FALSE;
}

int json_is_number(json_context_t *ctx, json_value_t *val) {
    if (!ctx || !val) return 0;
    if (ctx->is_number_fn) {
        return ctx->is_number_fn((void*)val);
    }
    return json_type(ctx, val) == JSON_TYPE_NUMBER;
}

int json_is_string(json_context_t *ctx, json_value_t *val) {
    if (!ctx || !val) return 0;
    if (ctx->is_string_fn) {
        return ctx->is_string_fn((void*)val);
    }
    return json_type(ctx, val) == JSON_TYPE_STRING;
}

int json_is_array(json_context_t *ctx, json_value_t *val) {
    if (!ctx || !val) return 0;
    if (ctx->is_array_fn) {
        return ctx->is_array_fn((void*)val);
    }
    return json_type(ctx, val) == JSON_TYPE_ARRAY;
}

int json_is_object(json_context_t *ctx, json_value_t *val) {
    if (!ctx || !val) return 0;
    if (ctx->is_object_fn) {
        return ctx->is_object_fn((void*)val);
    }
    return json_type(ctx, val) == JSON_TYPE_OBJECT;
}

/* ==================== Value Creation ==================== */

json_value_t* json_create_null(json_context_t *ctx) {
    if (!ctx || !ctx->create_null_fn) return NULL;
    return (json_value_t*)ctx->create_null_fn();
}

json_value_t* json_create_bool(json_context_t *ctx, int value) {
    if (!ctx) return NULL;
    if (ctx->create_bool_fn) {
        return (json_value_t*)ctx->create_bool_fn(value);
    }
    // Fallback
    if (value && ctx->create_true_fn) {
        return (json_value_t*)ctx->create_true_fn();
    }
    if (!value && ctx->create_false_fn) {
        return (json_value_t*)ctx->create_false_fn();
    }
    return NULL;
}

json_value_t* json_create_number(json_context_t *ctx, double value) {
    if (!ctx || !ctx->create_number_fn) return NULL;
    return (json_value_t*)ctx->create_number_fn(value);
}

json_value_t* json_create_string(json_context_t *ctx, const char *value) {
    if (!ctx || !ctx->create_string_fn || !value) return NULL;
    return (json_value_t*)ctx->create_string_fn(value);
}

json_value_t* json_create_array(json_context_t *ctx) {
    if (!ctx || !ctx->create_array_fn) return NULL;
    return (json_value_t*)ctx->create_array_fn();
}

json_value_t* json_create_object(json_context_t *ctx) {
    if (!ctx || !ctx->create_object_fn) return NULL;
    return (json_value_t*)ctx->create_object_fn();
}

/* ==================== Value Extraction ==================== */

int json_get_bool(json_context_t *ctx, json_value_t *val) {
    if (!ctx || !val) return 0;
    json_type_t t = json_type(ctx, val);
    return t == JSON_TYPE_TRUE ? 1 : 0;
}

double json_get_number(json_context_t *ctx, json_value_t *val) {
    if (!ctx || !val) return 0.0;
    cJSON_internal *item = (cJSON_internal*)val;
    return item->valuedouble;
}

const char* json_get_string(json_context_t *ctx, json_value_t *val) {
    if (!ctx || !val) return NULL;
    cJSON_internal *item = (cJSON_internal*)val;
    return item->valuestring;
}

/* ==================== Object Operations ==================== */

json_value_t* json_object_get(json_context_t *ctx, json_value_t *obj, const char *key) {
    if (!ctx || !ctx->get_object_item_fn || !obj || !key) return NULL;
    return (json_value_t*)ctx->get_object_item_fn((void*)obj, key);
}

int json_object_has(json_context_t *ctx, json_value_t *obj, const char *key) {
    if (!ctx || !obj || !key) return 0;
    if (ctx->has_object_item_fn) {
        return ctx->has_object_item_fn((void*)obj, key);
    }
    // Fallback: check if get returns non-NULL
    return json_object_get(ctx, obj, key) != NULL;
}

int json_object_set(json_context_t *ctx, json_value_t *obj, const char *key, json_value_t *val) {
    if (!ctx || !ctx->add_item_to_object_fn || !obj || !key || !val) return -1;
    return ctx->add_item_to_object_fn((void*)obj, key, (void*)val) ? 0 : -1;
}

void json_object_delete(json_context_t *ctx, json_value_t *obj, const char *key) {
    if (ctx && ctx->delete_item_from_object_fn && obj && key) {
        ctx->delete_item_from_object_fn((void*)obj, key);
    }
}

/* ==================== Array Operations ==================== */

int json_array_length(json_context_t *ctx, json_value_t *arr) {
    if (!ctx || !ctx->get_array_size_fn || !arr) return 0;
    return ctx->get_array_size_fn((void*)arr);
}

json_value_t* json_array_get(json_context_t *ctx, json_value_t *arr, int index) {
    if (!ctx || !ctx->get_array_item_fn || !arr || index < 0) return NULL;
    return (json_value_t*)ctx->get_array_item_fn((void*)arr, index);
}

int json_array_push(json_context_t *ctx, json_value_t *arr, json_value_t *val) {
    if (!ctx || !ctx->add_item_to_array_fn || !arr || !val) return -1;
    return ctx->add_item_to_array_fn((void*)arr, (void*)val) ? 0 : -1;
}

void json_array_delete(json_context_t *ctx, json_value_t *arr, int index) {
    if (ctx && ctx->delete_item_from_array_fn && arr && index >= 0) {
        ctx->delete_item_from_array_fn((void*)arr, index);
    }
}

/* ==================== Convenience Functions ==================== */

json_value_t* json_create_fmt(json_context_t *ctx, const char *fmt, ...) {
    if (!ctx || !fmt) return NULL;

    // 构建格式化字符串
    va_list args;
    va_start(args, fmt);

    // 计算所需大小
    va_list args_copy;
    va_copy(args_copy, args);
    int size = shim_vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (size < 0) {
        va_end(args);
        return NULL;
    }

    // 使用 System Layer API 分配和格式化
    char *buffer = (char*)shim_malloc(size + 1);
    if (!buffer) {
        va_end(args);
        return NULL;
    }

    shim_vsnprintf(buffer, size + 1, fmt, args);
    va_end(args);

    // 解析 JSON
    json_value_t *result = json_parse(ctx, buffer);
    shim_free(buffer);

    return result;
}
