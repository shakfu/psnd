/* async_queue.c - Unified event queue implementation
 *
 * Thread-safe ring buffer with libuv cross-thread notification.
 */

#include "async_queue.h"
#include "internal.h"
#include "buffers.h"
#include "lang_bridge.h"
#include <uv.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Lua headers for callback dispatch */
#include <lua.h>
#include <lauxlib.h>

/* ============================================================================
 * Queue Structure
 * ============================================================================ */

struct AsyncEventQueue {
    /* Ring buffer */
    AsyncEvent events[ASYNC_QUEUE_SIZE];
    _Atomic uint32_t head;          /* Consumer reads from here */
    _Atomic uint32_t tail;          /* Producer writes here */

    /* Cross-thread notification */
    uv_async_t wakeup;
    uv_mutex_t mutex;

    /* Handler dispatch table */
    AsyncEventHandler handlers[ASYNC_MAX_HANDLERS];

    /* Initialization state */
    int initialized;

    /* libuv loop (borrowed, not owned) */
    uv_loop_t *loop;
};

/* Global queue instance */
static AsyncEventQueue g_queue;

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static inline AsyncEventQueue *resolve_queue(AsyncEventQueue *queue) {
    return queue ? queue : (g_queue.initialized ? &g_queue : NULL);
}

/* Wake callback - does nothing, just wakes the event loop */
static void on_queue_wakeup(uv_async_t *handle) {
    (void)handle;
    /* The wakeup signal itself is sufficient - main thread will poll queue */
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

int async_queue_init(void) {
    if (g_queue.initialized) {
        return 0;  /* Already initialized */
    }

    memset(&g_queue, 0, sizeof(g_queue));

    /* Initialize atomic counters */
    atomic_store(&g_queue.head, 0);
    atomic_store(&g_queue.tail, 0);

    /* Initialize mutex */
    if (uv_mutex_init(&g_queue.mutex) != 0) {
        return -1;
    }

    /* Get or create default loop */
    g_queue.loop = uv_default_loop();
    if (!g_queue.loop) {
        uv_mutex_destroy(&g_queue.mutex);
        return -1;
    }

    /* Initialize async handle for cross-thread notification */
    if (uv_async_init(g_queue.loop, &g_queue.wakeup, on_queue_wakeup) != 0) {
        uv_mutex_destroy(&g_queue.mutex);
        return -1;
    }

    /* Register default handlers */
    g_queue.handlers[ASYNC_EVENT_LANG_CALLBACK] = async_handler_lang_callback;
    g_queue.handlers[ASYNC_EVENT_LINK_PEERS] = async_handler_link_peers;
    g_queue.handlers[ASYNC_EVENT_LINK_TEMPO] = async_handler_link_tempo;
    g_queue.handlers[ASYNC_EVENT_LINK_TRANSPORT] = async_handler_link_transport;
    g_queue.handlers[ASYNC_EVENT_BEAT_BOUNDARY] = async_handler_beat_boundary;

    g_queue.initialized = 1;

    return 0;
}

void async_queue_cleanup(void) {
    if (!g_queue.initialized) {
        return;
    }

    /* Stop accepting new events before tearing anything down. Taking the
     * lock here means any push already in flight has finished; pushes that
     * arrive afterwards fail in resolve_queue() instead of using a
     * destroyed mutex. */
    uv_mutex_lock(&g_queue.mutex);
    g_queue.initialized = 0;
    uv_mutex_unlock(&g_queue.mutex);

    /* Drain queue and free any heap data */
    AsyncEvent event;
    while (async_queue_poll(&g_queue, &event) == 0) {
        async_event_cleanup(&event);
    }

    /* Close async handle */
    uv_close((uv_handle_t *)&g_queue.wakeup, NULL);

    /* Run loop once to process close callbacks */
    uv_run(g_queue.loop, UV_RUN_NOWAIT);

    uv_mutex_destroy(&g_queue.mutex);
}

AsyncEventQueue *async_queue_global(void) {
    return g_queue.initialized ? &g_queue : NULL;
}

/* ============================================================================
 * Producer API (Thread-Safe)
 * ============================================================================ */

int async_queue_push(AsyncEventQueue *queue, const AsyncEvent *event) {
    queue = resolve_queue(queue);
    if (!queue || !event) {
        return -1;
    }

    uv_mutex_lock(&queue->mutex);

    uint32_t tail = atomic_load(&queue->tail);
    uint32_t next = (tail + 1) & ASYNC_QUEUE_SIZE_MASK;

    if (next == atomic_load(&queue->head)) {
        /* Queue full */
        uv_mutex_unlock(&queue->mutex);
        return -1;
    }

    /* Copy event into queue */
    queue->events[tail] = *event;

    /* Set timestamp if not already set */
    if (queue->events[tail].timestamp == 0) {
        queue->events[tail].timestamp = (int64_t)uv_hrtime();
    }

    atomic_store(&queue->tail, next);

    uv_mutex_unlock(&queue->mutex);

    /* Wake main thread */
    uv_async_send(&queue->wakeup);

    return 0;
}

int async_queue_push_lang_callback(AsyncEventQueue *queue, int slot_id, int status,
                                   int events_played, int duration_ms,
                                   const char *callback, const char *error) {
    AsyncEvent event = {
        .type = ASYNC_EVENT_LANG_CALLBACK,
        .flags = 0,
        .timestamp = (int64_t)uv_hrtime(),
        .data.lang = {
            .slot_id = slot_id,
            .status = status,
            .events_played = events_played,
            .duration_ms = duration_ms
        },
        .heap_data = NULL
    };
    /* Copy callback name */
    if (callback) {
        strncpy(event.data.lang.callback, callback, ASYNC_CALLBACK_NAME_SIZE - 1);
        event.data.lang.callback[ASYNC_CALLBACK_NAME_SIZE - 1] = '\0';
    } else {
        event.data.lang.callback[0] = '\0';
    }
    /* Copy error message */
    if (error) {
        strncpy(event.data.lang.error, error, ASYNC_ERROR_MSG_SIZE - 1);
        event.data.lang.error[ASYNC_ERROR_MSG_SIZE - 1] = '\0';
    } else {
        event.data.lang.error[0] = '\0';
    }
    return async_queue_push(queue, &event);
}

int async_queue_push_link_peers(AsyncEventQueue *queue, uint64_t peers, const char *callback) {
    AsyncEvent event = {
        .type = ASYNC_EVENT_LINK_PEERS,
        .flags = 0,
        .timestamp = (int64_t)uv_hrtime(),
        .data.link_peers = { .peers = peers },
        .heap_data = NULL
    };
    /* Copy callback name */
    if (callback) {
        strncpy(event.data.link_peers.callback, callback, ASYNC_CALLBACK_NAME_SIZE - 1);
        event.data.link_peers.callback[ASYNC_CALLBACK_NAME_SIZE - 1] = '\0';
    } else {
        event.data.link_peers.callback[0] = '\0';
    }
    return async_queue_push(queue, &event);
}

int async_queue_push_link_tempo(AsyncEventQueue *queue, double tempo, const char *callback) {
    AsyncEvent event = {
        .type = ASYNC_EVENT_LINK_TEMPO,
        .flags = 0,
        .timestamp = (int64_t)uv_hrtime(),
        .data.link_tempo = { .tempo = tempo },
        .heap_data = NULL
    };
    /* Copy callback name */
    if (callback) {
        strncpy(event.data.link_tempo.callback, callback, ASYNC_CALLBACK_NAME_SIZE - 1);
        event.data.link_tempo.callback[ASYNC_CALLBACK_NAME_SIZE - 1] = '\0';
    } else {
        event.data.link_tempo.callback[0] = '\0';
    }
    return async_queue_push(queue, &event);
}

int async_queue_push_link_transport(AsyncEventQueue *queue, int playing, const char *callback) {
    AsyncEvent event = {
        .type = ASYNC_EVENT_LINK_TRANSPORT,
        .flags = 0,
        .timestamp = (int64_t)uv_hrtime(),
        .data.link_transport = { .playing = playing },
        .heap_data = NULL
    };
    /* Copy callback name */
    if (callback) {
        strncpy(event.data.link_transport.callback, callback, ASYNC_CALLBACK_NAME_SIZE - 1);
        event.data.link_transport.callback[ASYNC_CALLBACK_NAME_SIZE - 1] = '\0';
    } else {
        event.data.link_transport.callback[0] = '\0';
    }
    return async_queue_push(queue, &event);
}

int async_queue_push_beat(AsyncEventQueue *queue, double beat, double quantum, int buffer_id) {
    AsyncEvent event = {
        .type = ASYNC_EVENT_BEAT_BOUNDARY,
        .flags = 0,
        .timestamp = (int64_t)uv_hrtime(),
        .data.beat = { .beat = beat, .quantum = quantum, .buffer_id = buffer_id },
        .heap_data = NULL
    };
    return async_queue_push(queue, &event);
}

int async_queue_push_timer(AsyncEventQueue *queue, int timer_id, void *userdata) {
    AsyncEvent event = {
        .type = ASYNC_EVENT_TIMER,
        .flags = 0,
        .timestamp = (int64_t)uv_hrtime(),
        .data.timer = { .timer_id = timer_id, .userdata = userdata },
        .heap_data = NULL
    };
    return async_queue_push(queue, &event);
}

int async_queue_push_custom(AsyncEventQueue *queue, const char *tag, const void *data, size_t len) {
    AsyncEvent event = {
        .type = ASYNC_EVENT_CUSTOM,
        .flags = 0,
        .timestamp = (int64_t)uv_hrtime(),
        .heap_data = NULL
    };

    /* Copy tag (truncate if needed) */
    if (tag) {
        strncpy(event.data.custom.tag, tag, ASYNC_CUSTOM_TAG_SIZE - 1);
        event.data.custom.tag[ASYNC_CUSTOM_TAG_SIZE - 1] = '\0';
    } else {
        event.data.custom.tag[0] = '\0';
    }

    /* Copy data to heap if provided */
    if (data && len > 0) {
        event.heap_data = malloc(len);
        if (!event.heap_data) {
            return -1;
        }
        memcpy(event.heap_data, data, len);
        event.data.custom.data = event.heap_data;
        event.data.custom.len = len;
    } else {
        event.data.custom.data = NULL;
        event.data.custom.len = 0;
    }

    int result = async_queue_push(queue, &event);
    if (result != 0 && event.heap_data) {
        free(event.heap_data);
    }
    return result;
}

/* ============================================================================
 * Consumer API (Main Thread Only)
 * ============================================================================ */

int async_queue_peek(AsyncEventQueue *queue, AsyncEvent *event) {
    queue = resolve_queue(queue);
    if (!queue || !event) {
        return 1;
    }

    uint32_t head = atomic_load(&queue->head);
    uint32_t tail = atomic_load(&queue->tail);

    if (head == tail) {
        return 1;  /* Empty */
    }

    *event = queue->events[head];
    /* Peek does not transfer ownership: the queue still owns any heap data,
     * and the next pop will free it. Do not hand out a pointer that is about
     * to dangle. */
    event->heap_data = NULL;
    return 0;
}

int async_queue_poll(AsyncEventQueue *queue, AsyncEvent *event) {
    queue = resolve_queue(queue);
    if (!queue || !event) {
        return 1;
    }

    uint32_t head = atomic_load(&queue->head);
    uint32_t tail = atomic_load(&queue->tail);

    if (head == tail) {
        return 1;  /* Empty */
    }

    *event = queue->events[head];
    atomic_store(&queue->head, (head + 1) & ASYNC_QUEUE_SIZE_MASK);

    return 0;
}

void async_queue_pop(AsyncEventQueue *queue) {
    queue = resolve_queue(queue);
    if (!queue) {
        return;
    }

    uint32_t head = atomic_load(&queue->head);
    uint32_t tail = atomic_load(&queue->tail);

    if (head == tail) {
        return;  /* Empty */
    }

    /* Free any heap data */
    async_event_cleanup(&queue->events[head]);

    atomic_store(&queue->head, (head + 1) & ASYNC_QUEUE_SIZE_MASK);
}

int async_queue_is_empty(AsyncEventQueue *queue) {
    queue = resolve_queue(queue);
    if (!queue) {
        return 1;
    }

    return atomic_load(&queue->head) == atomic_load(&queue->tail) ? 1 : 0;
}

int async_queue_count(AsyncEventQueue *queue) {
    queue = resolve_queue(queue);
    if (!queue) {
        return 0;
    }

    uint32_t head = atomic_load(&queue->head);
    uint32_t tail = atomic_load(&queue->tail);

    return (int)((tail - head) & ASYNC_QUEUE_SIZE_MASK);
}

int async_queue_dispatch_all(AsyncEventQueue *queue, void *ctx) {
    queue = resolve_queue(queue);
    if (!queue) {
        return 0;
    }

    int count = 0;
    AsyncEvent event;

    while (async_queue_poll(queue, &event) == 0) {
        if (event.type > ASYNC_EVENT_NONE && event.type < ASYNC_MAX_HANDLERS) {
            AsyncEventHandler handler = queue->handlers[event.type];
            if (handler) {
                handler(&event, ctx);
            }
        }
        async_event_cleanup(&event);
        count++;
    }

    return count;
}

int async_queue_dispatch_lua(AsyncEventQueue *queue, editor_ctx_t *ctx, lua_State *L) {
    queue = resolve_queue(queue);
    if (!queue) {
        return 0;
    }

    /* Create a context structure for handlers that need both ctx and L */
    struct {
        editor_ctx_t *ctx;
        lua_State *L;
    } dispatch_ctx = { ctx, L };

    return async_queue_dispatch_all(queue, &dispatch_ctx);
}

/* ============================================================================
 * Handler Registration
 * ============================================================================ */

void async_queue_set_handler(AsyncEventQueue *queue, AsyncEventType type, AsyncEventHandler handler) {
    queue = resolve_queue(queue);
    if (!queue || type <= ASYNC_EVENT_NONE || type >= ASYNC_MAX_HANDLERS) {
        return;
    }

    queue->handlers[type] = handler;
}

AsyncEventHandler async_queue_get_handler(AsyncEventQueue *queue, AsyncEventType type) {
    queue = resolve_queue(queue);
    if (!queue || type <= ASYNC_EVENT_NONE || type >= ASYNC_MAX_HANDLERS) {
        return NULL;
    }

    return queue->handlers[type];
}

/* ============================================================================
 * Default Handlers
 * ============================================================================ */

/* Context passed to handlers from async_queue_dispatch_lua */
typedef struct {
    editor_ctx_t *ctx;
    lua_State *L;
} DispatchContext;

void async_handler_lang_callback(AsyncEvent *event, void *ctx) {
    if (!event || event->type != ASYNC_EVENT_LANG_CALLBACK) {
        return;
    }

    DispatchContext *dc = (DispatchContext *)ctx;
    if (!dc || !dc->L) {
        return;
    }

    /* Skip if no callback registered */
    const char *callback = event->data.lang.callback;
    if (!callback || callback[0] == '\0') {
        return;
    }

    lua_State *L = dc->L;

    /* Get the callback function */
    lua_getglobal(L, callback);
    if (lua_isfunction(L, -1)) {
        /* Create result table for language callback */
        lua_newtable(L);

        /* status field */
        lua_pushstring(L, "status");
        switch (event->data.lang.status) {
            case 0:
                lua_pushstring(L, "complete");
                break;
            case 1:
                lua_pushstring(L, "stopped");
                break;
            case 2:
                lua_pushstring(L, "error");
                break;
            default:
                lua_pushstring(L, "unknown");
                break;
        }
        lua_settable(L, -3);

        /* slot field */
        lua_pushstring(L, "slot");
        lua_pushinteger(L, event->data.lang.slot_id);
        lua_settable(L, -3);

        /* events field */
        lua_pushstring(L, "events");
        lua_pushinteger(L, event->data.lang.events_played);
        lua_settable(L, -3);

        /* duration_ms field */
        lua_pushstring(L, "duration_ms");
        lua_pushinteger(L, event->data.lang.duration_ms);
        lua_settable(L, -3);

        /* error field (if present) */
        if (event->data.lang.error[0] != '\0') {
            lua_pushstring(L, "error");
            lua_pushstring(L, event->data.lang.error);
            lua_settable(L, -3);
        }

        /* Call the function with 1 argument (result table) */
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            const char *err = lua_tostring(L, -1);
            fprintf(stderr, "Language callback error: %s\n", err ? err : "unknown");
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);  /* Pop non-function value */
    }
}

void async_handler_link_peers(AsyncEvent *event, void *ctx) {
    if (!event || event->type != ASYNC_EVENT_LINK_PEERS) {
        return;
    }

    DispatchContext *dc = (DispatchContext *)ctx;
    if (!dc || !dc->L) {
        return;
    }

    /* Skip if no callback registered */
    const char *callback = event->data.link_peers.callback;
    if (!callback || callback[0] == '\0') {
        return;
    }

    lua_State *L = dc->L;
    lua_getglobal(L, callback);
    if (lua_isfunction(L, -1)) {
        lua_pushinteger(L, (lua_Integer)event->data.link_peers.peers);
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            const char *err = lua_tostring(L, -1);
            fprintf(stderr, "Link peers callback error: %s\n", err ? err : "unknown");
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);
    }
}

void async_handler_link_tempo(AsyncEvent *event, void *ctx) {
    if (!event || event->type != ASYNC_EVENT_LINK_TEMPO) {
        return;
    }

    DispatchContext *dc = (DispatchContext *)ctx;
    if (!dc || !dc->L) {
        return;
    }

    /* Skip if no callback registered */
    const char *callback = event->data.link_tempo.callback;
    if (!callback || callback[0] == '\0') {
        return;
    }

    lua_State *L = dc->L;
    lua_getglobal(L, callback);
    if (lua_isfunction(L, -1)) {
        lua_pushnumber(L, event->data.link_tempo.tempo);
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            const char *err = lua_tostring(L, -1);
            fprintf(stderr, "Link tempo callback error: %s\n", err ? err : "unknown");
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);
    }
}

void async_handler_link_transport(AsyncEvent *event, void *ctx) {
    if (!event || event->type != ASYNC_EVENT_LINK_TRANSPORT) {
        return;
    }

    DispatchContext *dc = (DispatchContext *)ctx;
    if (!dc || !dc->L) {
        return;
    }

    /* Skip if no callback registered */
    const char *callback = event->data.link_transport.callback;
    if (!callback || callback[0] == '\0') {
        return;
    }

    lua_State *L = dc->L;
    lua_getglobal(L, callback);
    if (lua_isfunction(L, -1)) {
        lua_pushboolean(L, event->data.link_transport.playing);
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            const char *err = lua_tostring(L, -1);
            fprintf(stderr, "Link start/stop callback error: %s\n", err ? err : "unknown");
            lua_pop(L, 1);
        }
    } else {
        lua_pop(L, 1);
    }
}

void async_handler_beat_boundary(AsyncEvent *event, void *ctx) {
    if (!event || event->type != ASYNC_EVENT_BEAT_BOUNDARY) {
        return;
    }

    DispatchContext *dc = (DispatchContext *)ctx;
    if (!dc) {
        return;
    }

    int buffer_id = event->data.beat.buffer_id;

    /* Get the buffer context and evaluate it */
    editor_ctx_t *buf_ctx = buffer_get(buffer_id);
    if (buf_ctx) {
        loki_lang_eval_buffer(buf_ctx);
    }
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

void async_event_cleanup(AsyncEvent *event) {
    if (!event) {
        return;
    }

    if (event->heap_data) {
        free(event->heap_data);
        event->heap_data = NULL;
    }

    /* Clear custom data pointer if it was pointing to heap */
    if (event->type == ASYNC_EVENT_CUSTOM) {
        event->data.custom.data = NULL;
        event->data.custom.len = 0;
    }
}

const char *async_event_type_name(AsyncEventType type) {
    switch (type) {
        case ASYNC_EVENT_NONE:           return "NONE";
        case ASYNC_EVENT_LANG_CALLBACK:  return "LANG_CALLBACK";
        case ASYNC_EVENT_LINK_PEERS:     return "LINK_PEERS";
        case ASYNC_EVENT_LINK_TEMPO:     return "LINK_TEMPO";
        case ASYNC_EVENT_LINK_TRANSPORT: return "LINK_TRANSPORT";
        case ASYNC_EVENT_TIMER:          return "TIMER";
        case ASYNC_EVENT_BEAT_BOUNDARY:  return "BEAT_BOUNDARY";
        case ASYNC_EVENT_CUSTOM:         return "CUSTOM";
        default:                         return "UNKNOWN";
    }
}
