#include "Cello.h"
#include "cdp_cello.h"

#include <stdlib.h> // For NULL

// 将 cdp_json_t* (即 void*) 安全地转换回 Cello 的 var 类型
// 这是一个内部使用的宏，以增强代码可读性
#define cdp_to_var(obj) ((var)(obj))

cdp_json_t* cdp_json_from_string(const char* json_string) {
    // 使用 Cello 加载 JSON 字符串。load 函数返回一个 Cello 的 var
    var json_var = load(Json, $(String, json_string));

    // Cello 在失败时会返回 Exception 对象，我们可以通过 type_of 来判断
    if (type_of(json_var) is Exception) {
        // 释放异常对象并返回 NULL
        del(json_var);
        return NULL;
    }

    // 将 Cello 的 var 直接作为 cdp_json_t* (void*) 返回
    return (cdp_json_t*)json_var;
}

void cdp_json_delete(cdp_json_t* obj) {
    if (!obj) return;
    // 将 void* 转回 var，并使用 Cello 的 del 函数释放
    del(cdp_to_var(obj));
}

const char* cdp_json_get_string(const cdp_json_t* obj, const char* key) {
    if (!obj || !key) return NULL;
    var value = get(cdp_to_var(obj), $(String, key));
    // 检查返回的是否是 String 类型
    if (type_of(value) is String) {
        return c_str(value);
    }
    return NULL;
}

int cdp_json_get_int(const cdp_json_t* obj, const char* key) {
    if (!obj || !key) return 0;
    var value = get(cdp_to_var(obj), $(String, key));
    // Cello 的 c_int 可以安全地处理 NULL 或非数值类型（返回0）
    return c_int(value);
}

cdp_json_t* cdp_json_get_object(const cdp_json_t* obj, const char* key) {
    if (!obj || !key) return NULL;
    var value = get(cdp_to_var(obj), $(String, key));
    // 检查返回的是否是 Table 类型 (JSON object)
    if (type_of(value) is Table) {
        return (cdp_json_t*)value;
    }
    return NULL;
}

cdp_json_t* cdp_json_get_array(const cdp_json_t* obj, const char* key) {
    if (!obj || !key) return NULL;
    var value = get(cdp_to_var(obj), $(String, key));
    // 检查返回的是否是 List 类型 (JSON array)
    if (type_of(value) is List) {
        return (cdp_json_t*)value;
    }
    return NULL;
}

int cdp_array_len(const cdp_json_t* arr) {
    if (!arr) return -1;
    if (type_of(cdp_to_var(arr)) is List) {
        return len(cdp_to_var(arr));
    }
    return -1;
}

const char* cdp_array_get_string(const cdp_json_t* arr, int index) {
    if (!arr) return NULL;
    var value = get(cdp_to_var(arr), $(Int, index));
    if (type_of(value) is String) {
        return c_str(value);
    }
    return NULL;
}

int cdp_array_get_int(const cdp_json_t* arr, int index) {
    if (!arr) return 0;
    var value = get(cdp_to_var(arr), $(Int, index));
    return c_int(value);
}

cdp_json_t* cdp_array_get_object(const cdp_json_t* arr, int index) {
    if (!arr) return NULL;
    var value = get(cdp_to_var(arr), $(Int, index));
    if (type_of(value) is Table) {
        return (cdp_json_t*)value;
    }
    return NULL;
}
