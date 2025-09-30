#ifndef CDP_CELLO_H
#define CDP_CELLO_H

// 使用一个不透明的 void 指针作为我们所有 JSON 对象的统一句柄。
// 它可以代表一个 JSON 对象、一个数组或任何其他 JSON 值。
// 外部代码只知道它是一个 cdp_json_t 指针，但不知道其内部是 Cello 的 var。
typedef void cdp_json_t;

/**
 * @brief 从 JSON 格式的字符串中解析出一个 JSON 对象句柄。
 *
 * @param json_string 包含有效 JSON 的 C 字符串。
 * @return cdp_json_t* 指向新创建的 JSON 对象的句柄。如果解析失败，返回 NULL。
 *         这个返回的对象是顶级对象，需要在使用完毕后调用 cdp_json_delete() 来释放。
 */
cdp_json_t* cdp_json_from_string(const char* json_string);

/**
 * @brief 释放由 cdp_json_from_string 创建的顶级 JSON 对象。
 *
 * @param obj 指向顶级 JSON 对象的句柄。
 */
void cdp_json_delete(cdp_json_t* obj);

/**
 * @brief 从一个 JSON 对象中根据 key 获取一个字符串值。
 *
 * @param obj 指向 JSON 对象的句柄。
 * @param key 要查询的 C 字符串 key。
 * @return const char* 指向值的 C 字符串。如果 key 不存在或值的类型不匹配，返回 NULL。
 *         返回的字符串由库管理，不应被修改或释放。
 */
const char* cdp_json_get_string(const cdp_json_t* obj, const char* key);

/**
 * @brief 从一个 JSON 对象中根据 key 获取一个整数值。
 *
 * @param obj 指向 JSON 对象的句柄。
 * @param key 要查询的 C 字符串 key。
 * @return int 整数值。如果 key 不存在或值的类型不匹配，通常返回 0。
 */
int cdp_json_get_int(const cdp_json_t* obj, const char* key);

/**
 * @brief 从一个 JSON 对象中根据 key 获取一个嵌套的 JSON 对象。
 *
 * @param obj 指向 JSON 对象的句柄。
 * @param key 要查询的 C 字符串 key。
 * @return cdp_json_t* 指向嵌套对象的句柄。如果 key 不存在或值的类型不匹配，返回 NULL。
 *         返回的句柄是临时的，其生命周期由父对象管理，不应单独删除。
 */
cdp_json_t* cdp_json_get_object(const cdp_json_t* obj, const char* key);

/**
 * @brief 从一个 JSON 对象中根据 key 获取一个 JSON 数组。
 *
 * @param obj 指向 JSON 对象的句柄。
 * @param key 要查询的 C 字符串 key。
 * @return cdp_json_t* 指向数组的句柄。如果 key 不存在或值的类型不匹配，返回 NULL。
 *         返回的句柄是临时的，其生命周期由父对象管理，不应单独删除。
 */
cdp_json_t* cdp_json_get_array(const cdp_json_t* obj, const char* key);

/**
 * @brief 获取一个 JSON 数组的长度。
 *
 * @param arr 指向 JSON 数组的句柄。
 * @return int 数组中的元素数量。如果句柄不是一个数组，返回 -1。
 */
int cdp_array_len(const cdp_json_t* arr);

/**
 * @brief 从 JSON 数组的指定索引处获取一个字符串。
 *
 * @param arr 指向 JSON 数组的句柄。
 * @param index 数组中的索引（从 0 开始）。
 * @return const char* 指向值的 C 字符串。如果索引越界或类型不匹配，返回 NULL。
 */
const char* cdp_array_get_string(const cdp_json_t* arr, int index);

/**
 * @brief 从 JSON 数组的指定索引处获取一个整数。
 *
 * @param arr 指向 JSON 数组的句柄。
 * @param index 数组中的索引（从 0 开始）。
 * @return int 整数值。如果索引越界或类型不匹配，通常返回 0。
 */
int cdp_array_get_int(const cdp_json_t* arr, int index);

/**
 * @brief 从 JSON 数组的指定索引处获取一个嵌套的对象。
 *
 * @param arr 指向 JSON 数组的句柄。
 * @param index 数组中的索引（从 0 开始）。
 * @return cdp_json_t* 指向嵌套对象的句柄。如果索引越界或类型不匹配，返回 NULL。
 */
cdp_json_t* cdp_array_get_object(const cdp_json_t* arr, int index);

#endif // CDP_CELLO_H
