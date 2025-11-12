/*
 * mod_events.c - EventEmitter implementation
 *
 * 使用 System Layer API 和 mod_ds 容器实现事件发射器
 * EventEmitter implementation using System Layer API and mod_ds containers
 */

#include "mod_events.h"

/* ==================== Internal Structures ==================== */

/* 内部监听器节点结构 */
typedef struct event_listener {
    event_listener_fn callback;
    void *ctx;
    int once;  /* 1 if this is a one-time listener */
    struct event_listener *next;
} event_listener_t;

/* EventEmitter 结构 */
struct event_emitter {
    ds_map_t *events;  /* 映射事件名称 -> 监听器链表 */
};

/* ==================== Helper Functions ==================== */

/* 创建新的监听器节点 */
static event_listener_t* listener_new(event_listener_fn callback, void *ctx, int once) {
    event_listener_t *listener = (event_listener_t*)shim_malloc(sizeof(event_listener_t));
    if (!listener) return NULL;

    listener->callback = callback;
    listener->ctx = ctx;
    listener->once = once;
    listener->next = NULL;

    return listener;
}

/* 释放监听器节点 */
static void listener_free(event_listener_t *listener) {
    shim_free(listener);
}

/* Free entire listener list */
static void listener_list_free(event_listener_t *head) {
    while (head) {
        event_listener_t *next = head->next;
        listener_free(head);
        head = next;
    }
}

/* Count listeners in a list */
static size_t listener_list_count(event_listener_t *head) {
    size_t count = 0;
    while (head) {
        count++;
        head = head->next;
    }
    return count;
}

/* Add listener to list (at end to preserve order) */
static event_listener_t* listener_list_append(event_listener_t *head,
                                               event_listener_fn callback,
                                               void *ctx, int once) {
    event_listener_t *new_listener = listener_new(callback, ctx, once);
    if (!new_listener) return head;

    if (!head) {
        return new_listener;
    }

    // Find tail
    event_listener_t *tail = head;
    while (tail->next) {
        tail = tail->next;
    }

    tail->next = new_listener;
    return head;
}

/* Remove listener from list by callback function pointer */
static event_listener_t* listener_list_remove(event_listener_t *head,
                                               event_listener_fn callback) {
    if (!head) return NULL;

    // Handle head removal
    if (head->callback == callback) {
        event_listener_t *next = head->next;
        listener_free(head);
        return next;
    }

    // Find and remove in middle/tail
    event_listener_t *prev = head;
    event_listener_t *curr = head->next;

    while (curr) {
        if (curr->callback == callback) {
            prev->next = curr->next;
            listener_free(curr);
            return head;
        }
        prev = curr;
        curr = curr->next;
    }

    return head;  // Not found
}

/* ==================== Public API Implementation ==================== */

event_emitter_t* event_emitter_new(void) {
    event_emitter_t *emitter = (event_emitter_t*)shim_malloc(sizeof(event_emitter_t));
    if (!emitter) return NULL;

    /* 使用 mod_ds 中的 ds_map_new_with，支持字符串键比较 */
    emitter->events = ds_map_new_with(ds_hash_string, ds_compare_string);
    if (!emitter->events) {
        shim_free(emitter);
        return NULL;
    }

    return emitter;
}

void event_emitter_free(event_emitter_t *emitter) {
    if (!emitter) return;

    /* 释放所有监听器列表 */
    if (emitter->events) {
        /* 遍历 ds_map 的所有条目，释放监听器链表 */
        for (size_t i = 0; i < emitter->events->capacity; i++) {
            ds_map_entry_t *entry = emitter->events->buckets[i];
            while (entry) {
                if (entry->value) {
                    listener_list_free((event_listener_t*)entry->value);
                }
                entry = entry->next;
            }
        }

        ds_map_free(emitter->events);
    }

    shim_free(emitter);
}

int event_on(event_emitter_t *emitter, const char *event,
             event_listener_fn listener, void *ctx) {
    if (!emitter || !event || !listener) return -1;

    /* 从 ds_map 获取现有的监听器列表 */
    event_listener_t *list = (event_listener_t*)ds_map_get(emitter->events, event);
    event_listener_t *new_list = listener_list_append(list, listener, ctx, 0);

    if (!new_list && !list) return -1;  /* 内存分配失败 */

    ds_map_put(emitter->events, (void*)event, new_list);
    return 0;
}

int event_once(event_emitter_t *emitter, const char *event,
               event_listener_fn listener, void *ctx) {
    if (!emitter || !event || !listener) return -1;

    /* 获取现有的监听器列表，添加一次性监听器 */
    event_listener_t *list = (event_listener_t*)ds_map_get(emitter->events, event);
    event_listener_t *new_list = listener_list_append(list, listener, ctx, 1);

    if (!new_list && !list) return -1;  /* 内存分配失败 */

    ds_map_put(emitter->events, (void*)event, new_list);
    return 0;
}

int event_off(event_emitter_t *emitter, const char *event,
              event_listener_fn listener) {
    if (!emitter || !event || !listener) return -1;

    /* 获取并移除指定的监听器 */
    event_listener_t *list = (event_listener_t*)ds_map_get(emitter->events, event);
    if (!list) return -1;  /* 该事件没有监听器 */

    event_listener_t *new_list = listener_list_remove(list, listener);

    if (!new_list) {
        /* 列表已为空，从 ds_map 中移除该事件 */
        ds_map_remove(emitter->events, event);
    } else {
        ds_map_put(emitter->events, (void*)event, new_list);
    }

    return 0;
}

int event_emit(event_emitter_t *emitter, const char *event, void *data) {
    if (!emitter || !event) return 0;

    /* 从 ds_map 中获取监听器列表 */
    event_listener_t *list = (event_listener_t*)ds_map_get(emitter->events, event);
    if (!list) return 0;  /* 没有监听器 */

    int count = 0;
    event_listener_t *curr = list;
    event_listener_t *prev = NULL;
    event_listener_t *new_head = list;

    while (curr) {
        event_listener_t *next = curr->next;

        /* 调用监听器回调函数 */
        curr->callback(event, data, curr->ctx);
        count++;

        /* 如果是一次性监听器，则移除 */
        if (curr->once) {
            if (prev) {
                prev->next = next;
            } else {
                new_head = next;
            }
            listener_free(curr);
        } else {
            prev = curr;
        }

        curr = next;
    }

    /* 更新 ds_map 中的列表（如果一次性监听器被移除） */
    if (!new_head) {
        ds_map_remove(emitter->events, event);
    } else if (new_head != list) {
        ds_map_put(emitter->events, (void*)event, new_head);
    }

    return count;
}

size_t event_listener_count(event_emitter_t *emitter, const char *event) {
    if (!emitter || !event) return 0;

    /* 从 ds_map 中获取监听器列表，然后计数 */
    event_listener_t *list = (event_listener_t*)ds_map_get(emitter->events, event);
    return listener_list_count(list);
}

int event_remove_all_listeners(event_emitter_t *emitter, const char *event) {
    if (!emitter) return 0;

    int count = 0;

    if (event) {
        /* 移除指定事件的所有监听器 */
        event_listener_t *list = (event_listener_t*)ds_map_get(emitter->events, event);
        if (list) {
            count = listener_list_count(list);
            listener_list_free(list);
            ds_map_remove(emitter->events, event);
        }
    } else {
        /* 移除所有事件的所有监听器 */
        for (size_t i = 0; i < emitter->events->capacity; i++) {
            ds_map_entry_t *entry = emitter->events->buckets[i];
            while (entry) {
                if (entry->value) {
                    event_listener_t *list = (event_listener_t*)entry->value;
                    count += listener_list_count(list);
                    listener_list_free(list);
                    entry->value = NULL;
                }
                entry = entry->next;
            }
        }

        /* 清空 ds_map */
        ds_map_free(emitter->events);
        emitter->events = ds_map_new_with(ds_hash_string, ds_compare_string);
    }

    return count;
}

/* 收集事件名称的辅助结构 */
typedef struct {
    char **names;
    size_t count;
    size_t capacity;
} event_names_collector_t;

/* 收集事件名称的回调函数 */
static void collect_event_name(const char *key, void *value, void *userdata) {
    event_names_collector_t *collector = (event_names_collector_t*)userdata;

    /* 只收集有监听器的事件 */
    if (!value) return;

    event_listener_t *list = (event_listener_t*)value;
    if (!list) return;

    /* 如果需要，扩展数组容量 */
    if (collector->count >= collector->capacity) {
        size_t new_capacity = collector->capacity * 2;
        char **new_names = (char**)shim_realloc(collector->names,
                                           sizeof(char*) * (new_capacity + 1));
        if (!new_names) return;
        collector->names = new_names;
        collector->capacity = new_capacity;
    }

    /* 添加事件名称 */
    collector->names[collector->count] = shim_strdup(key);
    if (collector->names[collector->count]) {
        collector->count++;
    }
}

char** event_names(event_emitter_t *emitter, size_t *count) {
    if (!emitter || !count) return NULL;

    *count = 0;

    /* 初始化收集器 */
    event_names_collector_t collector;
    collector.capacity = 16;
    collector.count = 0;
    collector.names = (char**)shim_malloc(sizeof(char*) * (collector.capacity + 1));
    if (!collector.names) return NULL;

    /* 收集所有事件名称 - 遍历 ds_map */
    for (size_t i = 0; i < emitter->events->capacity; i++) {
        ds_map_entry_t *entry = emitter->events->buckets[i];
        while (entry) {
            collect_event_name((const char*)entry->key, entry->value, &collector);
            entry = entry->next;
        }
    }

    /* NULL-终止数组 */
    collector.names[collector.count] = NULL;

    *count = collector.count;
    return collector.names;
}
