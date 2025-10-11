// mod_json.c - JSON parsing and serialization for cosmorun
// Provides cJSON wrapper with dynamic loading support
//

#define NULL (void*)0

#ifndef RTLD_LAZY
#define RTLD_LAZY 1
#endif
#ifndef RTLD_GLOBAL
#define RTLD_GLOBAL 256
#endif

#include "mod_json.h"

/* ==================== Internal cJSON Structures ==================== */

// Minimal cJSON structure definition for type checking
// Must match cJSON.h layout
typedef struct cJSON_internal {
    struct cJSON_internal *next;
    struct cJSON_internal *prev;
    struct cJSON_internal *child;
    int type;
    char *valuestring;
    int valueint;
    double valuedouble;
    char *string;
} cJSON_internal;

// cJSON type flags (from cJSON.h)
#define cJSON_Invalid (0)
#define cJSON_False  (1 << 0)
#define cJSON_True   (1 << 1)
#define cJSON_NULL   (1 << 2)
#define cJSON_Number (1 << 3)
#define cJSON_String (1 << 4)
#define cJSON_Array  (1 << 5)
#define cJSON_Object (1 << 6)

/* ==================== Library Auto-detection ==================== */

static int json_preferred_dlopen_flags(void) {
#if defined(_WIN32) || defined(_WIN64)
    return RTLD_LAZY;
#else
    return RTLD_LAZY | RTLD_GLOBAL;
#endif
}

static void *json_try_dlopen(const char *path) {
    if (!path || !*path) {
        return NULL;
    }

    int flags = json_preferred_dlopen_flags();
    void *handle = dlopen(path, flags);
    if (handle) {
        return handle;
    }

    return dlopen(path, 0);
}

static void *json_dlopen_auto(const char *requested_path) {
    static const char *const candidates[] = {
#if defined(_WIN32) || defined(_WIN64)
        "lib/libcjson.dll",
        "lib/cjson.dll",
        "./cjson.dll",
        "cjson.dll",
        "./libcjson.dll",
        "libcjson.dll",
#elif defined(__APPLE__)
        "lib/libcjson.dylib",
        "./libcjson.dylib",
        "libcjson.dylib",
        "/usr/local/lib/libcjson.dylib",
        "/opt/homebrew/lib/libcjson.dylib",
#else
        "lib/libcjson.so",
        "./libcjson.so",
        "libcjson.so",
        "/usr/lib/libcjson.so",
        "/usr/local/lib/libcjson.so",
        "./cjson.so",
        "cjson.so",
        "lib/libcjson.so.1",
        "/usr/lib/libcjson.so.1",
#endif
        NULL
    };

    if (requested_path && *requested_path) {
        void *handle = json_try_dlopen(requested_path);
        if (handle) {
            return handle;
        }
    }

    for (const char *const *cand = candidates; *cand; ++cand) {
        if (requested_path && *requested_path && strcmp(requested_path, *cand) == 0) {
            continue;
        }
        void *handle = json_try_dlopen(*cand);
        if (handle) {
            return handle;
        }
    }
    return NULL;
}

/* ==================== Context Management ==================== */

json_context_t* json_init(const char *lib_path) {
    void *handle = json_dlopen_auto(lib_path ? lib_path : "");
    if (!handle) {
        return NULL;
    }

    json_context_t *ctx = (json_context_t*)malloc(sizeof(json_context_t));
    if (!ctx) {
        dlclose(handle);
        return NULL;
    }

    ctx->lib_handle = handle;

    // Load function pointers from cJSON library
    ctx->parse_fn = dlsym(handle, "cJSON_Parse");
    ctx->parse_with_length_fn = dlsym(handle, "cJSON_ParseWithLength");
    ctx->print_fn = dlsym(handle, "cJSON_Print");
    ctx->print_unformatted_fn = dlsym(handle, "cJSON_PrintUnformatted");
    ctx->delete_fn = dlsym(handle, "cJSON_Delete");
    ctx->free_fn = dlsym(handle, "cJSON_free");

    // Object operations
    ctx->create_object_fn = dlsym(handle, "cJSON_CreateObject");
    ctx->get_object_item_fn = dlsym(handle, "cJSON_GetObjectItem");
    ctx->has_object_item_fn = dlsym(handle, "cJSON_HasObjectItem");
    ctx->add_item_to_object_fn = dlsym(handle, "cJSON_AddItemToObject");
    ctx->detach_item_from_object_fn = dlsym(handle, "cJSON_DetachItemFromObject");
    ctx->delete_item_from_object_fn = dlsym(handle, "cJSON_DeleteItemFromObject");

    // Array operations
    ctx->create_array_fn = dlsym(handle, "cJSON_CreateArray");
    ctx->get_array_size_fn = dlsym(handle, "cJSON_GetArraySize");
    ctx->get_array_item_fn = dlsym(handle, "cJSON_GetArrayItem");
    ctx->add_item_to_array_fn = dlsym(handle, "cJSON_AddItemToArray");
    ctx->detach_item_from_array_fn = dlsym(handle, "cJSON_DetachItemFromArray");
    ctx->delete_item_from_array_fn = dlsym(handle, "cJSON_DeleteItemFromArray");

    // Value creation
    ctx->create_null_fn = dlsym(handle, "cJSON_CreateNull");
    ctx->create_true_fn = dlsym(handle, "cJSON_CreateTrue");
    ctx->create_false_fn = dlsym(handle, "cJSON_CreateFalse");
    ctx->create_bool_fn = dlsym(handle, "cJSON_CreateBool");
    ctx->create_number_fn = dlsym(handle, "cJSON_CreateNumber");
    ctx->create_string_fn = dlsym(handle, "cJSON_CreateString");

    // Value inspection
    ctx->is_null_fn = dlsym(handle, "cJSON_IsNull");
    ctx->is_false_fn = dlsym(handle, "cJSON_IsFalse");
    ctx->is_true_fn = dlsym(handle, "cJSON_IsTrue");
    ctx->is_bool_fn = dlsym(handle, "cJSON_IsBool");
    ctx->is_number_fn = dlsym(handle, "cJSON_IsNumber");
    ctx->is_string_fn = dlsym(handle, "cJSON_IsString");
    ctx->is_array_fn = dlsym(handle, "cJSON_IsArray");
    ctx->is_object_fn = dlsym(handle, "cJSON_IsObject");

    // Verify essential functions loaded
    if (!ctx->parse_fn || !ctx->print_fn || !ctx->delete_fn) {
        free(ctx);
        dlclose(handle);
        return NULL;
    }

    return ctx;
}

void json_cleanup(json_context_t *ctx) {
    if (!ctx) return;

    if (ctx->lib_handle) {
        dlclose(ctx->lib_handle);
        ctx->lib_handle = NULL;
    }

    free(ctx);
}

/* ==================== Parsing & Serialization ==================== */

json_value_t* json_parse(json_context_t *ctx, const char *str) {
    if (!ctx || !ctx->parse_fn || !str) return NULL;
    return (json_value_t*)ctx->parse_fn(str);
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
            free(str);
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

    // Build formatted string
    va_list args;
    va_start(args, fmt);

    // Calculate needed size
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (size < 0) {
        va_end(args);
        return NULL;
    }

    // Allocate and format
    char *buffer = (char*)malloc(size + 1);
    if (!buffer) {
        va_end(args);
        return NULL;
    }

    vsnprintf(buffer, size + 1, fmt, args);
    va_end(args);

    // Parse JSON
    json_value_t *result = json_parse(ctx, buffer);
    free(buffer);

    return result;
}
