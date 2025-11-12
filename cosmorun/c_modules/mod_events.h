#include "../cosmorun_system/libc_shim/sys_libc_shim.h"
#ifndef COSMORUN_EVENTS_H
#define COSMORUN_EVENTS_H

/*
 * mod_events - Node.js-style EventEmitter for cosmorun
 *
 * 提供事件驱动编程，支持多个监听器、一次性监听器和监听器管理。
 * Provides event-driven programming with multiple listeners,
 * one-time listeners, and listener management.
 *
 * API mirrors Node.js EventEmitter:
 * - on(event, listener): Add persistent listener
 * - once(event, listener): Add one-time listener
 * - off(event, listener): Remove specific listener
 * - emit(event, data): Trigger all listeners for event
 * - listenerCount(event): Count listeners for event
 * - removeAllListeners([event]): Remove all (or event-specific) listeners
 * - eventNames(): Get array of registered event names
 */

#include "mod_ds.h"
/* size_t and other basic types provided by sys_libc_shim.h via mod_ds.h */

/* ==================== Types ==================== */

/* Listener callback function
 * @param event: Event name that was emitted
 * @param data: Event data payload (can be NULL)
 * @param ctx: User context passed when listener was registered
 */
typedef void (*event_listener_fn)(const char *event, void *data, void *ctx);

/* EventEmitter structure (opaque) */
typedef struct event_emitter event_emitter_t;

/* ==================== Core API ==================== */

/* Create a new EventEmitter instance
 * @return: New EventEmitter or NULL on allocation failure
 */
event_emitter_t* event_emitter_new(void);

/* Free EventEmitter and all associated listeners
 * @param emitter: EventEmitter to free
 */
void event_emitter_free(event_emitter_t *emitter);

/* Add a persistent event listener
 * @param emitter: EventEmitter instance
 * @param event: Event name to listen for
 * @param listener: Callback function
 * @param ctx: User context (optional, can be NULL)
 * @return: 0 on success, -1 on failure
 */
int event_on(event_emitter_t *emitter, const char *event,
             event_listener_fn listener, void *ctx);

/* Add a one-time event listener (removed after first trigger)
 * @param emitter: EventEmitter instance
 * @param event: Event name to listen for
 * @param listener: Callback function
 * @param ctx: User context (optional, can be NULL)
 * @return: 0 on success, -1 on failure
 */
int event_once(event_emitter_t *emitter, const char *event,
               event_listener_fn listener, void *ctx);

/* Remove a specific event listener
 * @param emitter: EventEmitter instance
 * @param event: Event name
 * @param listener: Callback function to remove
 * @return: 0 on success, -1 if not found
 */
int event_off(event_emitter_t *emitter, const char *event,
              event_listener_fn listener);

/* Emit an event, calling all registered listeners
 * @param emitter: EventEmitter instance
 * @param event: Event name to emit
 * @param data: Event data payload (optional, can be NULL)
 * @return: Number of listeners that were called
 */
int event_emit(event_emitter_t *emitter, const char *event, void *data);

/* ==================== Utility API ==================== */

/* Count listeners for a specific event
 * @param emitter: EventEmitter instance
 * @param event: Event name
 * @return: Number of listeners registered for event
 */
size_t event_listener_count(event_emitter_t *emitter, const char *event);

/* Remove all listeners for a specific event, or all events if event is NULL
 * @param emitter: EventEmitter instance
 * @param event: Event name (NULL to remove all listeners)
 * @return: Number of listeners removed
 */
int event_remove_all_listeners(event_emitter_t *emitter, const char *event);

/* Get array of all registered event names
 * @param emitter: EventEmitter instance
 * @param count: Output parameter for number of event names
 * @return: NULL-terminated array of event name strings (must be freed by caller)
 *          Returns NULL if no events or on error
 */
char** event_names(event_emitter_t *emitter, size_t *count);

#endif /* COSMORUN_EVENTS_H */
