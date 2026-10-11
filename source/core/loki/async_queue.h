/* async_queue.h - Unified event queue for async tasks
 *
 * Provides a thread-safe event queue for delivering asynchronous events
 * from background threads to the main thread. Uses libuv for cross-thread
 * notification.
 *
 * Features:
 * - Lock-free SPSC or mutex-protected MPSC ring buffer
 * - uv_async_t for waking the main thread
 * - Type-safe event structures with union for event-specific data
 * - Extensible handler registration
 */

#ifndef LOKI_ASYNC_QUEUE_H
#define LOKI_ASYNC_QUEUE_H

#include <stdint.h>
#include <stddef.h>
#if defined(_MSC_VER) && !defined(__cplusplus)
/* MSVC C mode: use volatile for simple atomics.
 * On x86/x64, aligned 32-bit reads/writes are atomic. */
#define ASYNC_ATOMIC_SIZE_T volatile size_t
#define ASYNC_ATOMIC_UINT32_T volatile uint32_t
#define _Atomic volatile  /* Make _Atomic a no-op */
#ifndef atomic_store
#define atomic_store(ptr, val) (*(ptr) = (val))
#endif
#ifndef atomic_load
#define atomic_load(ptr) (*(ptr))
#endif
#else
#include <stdatomic.h>
#define ASYNC_ATOMIC_SIZE_T _Atomic size_t
#define ASYNC_ATOMIC_UINT32_T _Atomic uint32_t
#endif

/* Forward declarations */
struct lua_State;
typedef struct editor_ctx editor_ctx_t;

/* ============================================================================
 * Event Types
 * ============================================================================ */

typedef enum {
    ASYNC_EVENT_NONE = 0,

    /* Language events */
    ASYNC_EVENT_LANG_CALLBACK,      /* Playback completed */

    /* Link events */
    ASYNC_EVENT_LINK_PEERS,         /* Peer count changed */
    ASYNC_EVENT_LINK_TEMPO,         /* Tempo changed */
    ASYNC_EVENT_LINK_TRANSPORT,     /* Start/stop changed */

    /* Timer events */
    ASYNC_EVENT_TIMER,              /* User-scheduled timer fired */
    ASYNC_EVENT_BEAT_BOUNDARY,      /* Beat quantum crossed (live loop) */

    /* Future extensibility */
    ASYNC_EVENT_CUSTOM,             /* User-defined via Lua */

    ASYNC_EVENT_TYPE_COUNT          /* Must be last */
} AsyncEventType;

/* ============================================================================
 * Event Structure
 * ============================================================================ */

#define ASYNC_CUSTOM_TAG_SIZE 16
#define ASYNC_CALLBACK_NAME_SIZE 64
#define ASYNC_ERROR_MSG_SIZE 128

typedef struct AsyncEvent {
    AsyncEventType type;
    uint32_t flags;
    int64_t timestamp;              /* uv_hrtime() at push */

    union {
        /* ASYNC_EVENT_LANG_CALLBACK */
        struct {
            int slot_id;
            int status;             /* 0=complete, 1=stopped, 2=error */
            int events_played;      /* Number of events played */
            int duration_ms;        /* Playback duration */
            char callback[ASYNC_CALLBACK_NAME_SIZE];  /* Lua callback name */
            char error[ASYNC_ERROR_MSG_SIZE];         /* Error message if any */
        } lang;

        /* ASYNC_EVENT_LINK_PEERS */
        struct {
            uint64_t peers;
            char callback[ASYNC_CALLBACK_NAME_SIZE];  /* Lua callback name */
        } link_peers;

        /* ASYNC_EVENT_LINK_TEMPO */
        struct {
            double tempo;
            char callback[ASYNC_CALLBACK_NAME_SIZE];  /* Lua callback name */
        } link_tempo;

        /* ASYNC_EVENT_LINK_TRANSPORT */
        struct {
            int playing;
            char callback[ASYNC_CALLBACK_NAME_SIZE];  /* Lua callback name */
        } link_transport;

        /* ASYNC_EVENT_TIMER */
        struct {
            int timer_id;
            void *userdata;
        } timer;

        /* ASYNC_EVENT_BEAT_BOUNDARY */
        struct {
            double beat;
            double quantum;
            int buffer_id;
        } beat;

        /* ASYNC_EVENT_CUSTOM */
        struct {
            char tag[ASYNC_CUSTOM_TAG_SIZE];
            void *data;
            size_t len;
        } custom;
    } data;

    void *heap_data;                /* Non-NULL if data on heap, freed on pop.
                                     * Always NULL in an event returned by
                                     * async_queue_peek(), which does not
                                     * transfer ownership. */
} AsyncEvent;

/* ============================================================================
 * Queue Configuration
 * ============================================================================ */

#define ASYNC_QUEUE_SIZE 256        /* Must be power of 2 */
#define ASYNC_QUEUE_SIZE_MASK (ASYNC_QUEUE_SIZE - 1)
#define ASYNC_MAX_HANDLERS 16

/* ============================================================================
 * Event Handler Type
 * ============================================================================ */

typedef void (*AsyncEventHandler)(AsyncEvent *event, void *ctx);

/* ============================================================================
 * Queue Structure (opaque - implementation in async_queue.c)
 * ============================================================================ */

typedef struct AsyncEventQueue AsyncEventQueue;

/* ============================================================================
 * Lifecycle API
 * ============================================================================ */

/**
 * Initialize the global async event queue.
 * Must be called once before using any other async_queue functions.
 * Safe to call multiple times (no-op after first init).
 *
 * @return 0 on success, -1 on error
 */
int async_queue_init(void);

/**
 * Clean up the global async event queue.
 * Should be called during shutdown to free resources.
 */
void async_queue_cleanup(void);

/**
 * Get the global async event queue instance.
 *
 * @return Pointer to the global queue, or NULL if not initialized
 */
AsyncEventQueue *async_queue_global(void);

/* ============================================================================
 * Producer API (Thread-Safe)
 * ============================================================================
 *
 * These functions can be called from any thread. They use mutex protection
 * and uv_async_send to notify the main thread.
 */

/**
 * Push a generic event to the queue.
 *
 * @param queue The event queue (or NULL for global)
 * @param event The event to push (copied into queue)
 * @return 0 on success, -1 if queue is full
 */
int async_queue_push(AsyncEventQueue *queue, const AsyncEvent *event);

/**
 * Push a language callback completion event.
 *
 * @param queue The event queue (or NULL for global)
 * @param slot_id The playback slot that completed
 * @param status 0=complete, 1=stopped, 2=error
 * @param events_played Number of events played
 * @param duration_ms Playback duration in milliseconds
 * @param callback Lua callback function name (can be NULL)
 * @param error Error message if status is error (can be NULL)
 * @return 0 on success, -1 if queue is full
 */
int async_queue_push_lang_callback(AsyncEventQueue *queue, int slot_id, int status,
                                   int events_played, int duration_ms,
                                   const char *callback, const char *error);

/**
 * Push a Link peer count change event.
 *
 * @param queue The event queue (or NULL for global)
 * @param peers New peer count
 * @param callback Lua callback function name (can be NULL)
 * @return 0 on success, -1 if queue is full
 */
int async_queue_push_link_peers(AsyncEventQueue *queue, uint64_t peers, const char *callback);

/**
 * Push a Link tempo change event.
 *
 * @param queue The event queue (or NULL for global)
 * @param tempo New tempo in BPM
 * @param callback Lua callback function name (can be NULL)
 * @return 0 on success, -1 if queue is full
 */
int async_queue_push_link_tempo(AsyncEventQueue *queue, double tempo, const char *callback);

/**
 * Push a Link transport (start/stop) change event.
 *
 * @param queue The event queue (or NULL for global)
 * @param playing 1 if playing, 0 if stopped
 * @param callback Lua callback function name (can be NULL)
 * @return 0 on success, -1 if queue is full
 */
int async_queue_push_link_transport(AsyncEventQueue *queue, int playing, const char *callback);

/**
 * Push a beat boundary event (for live loops).
 *
 * @param queue The event queue (or NULL for global)
 * @param beat Current beat position
 * @param quantum Beat interval (quantum)
 * @param buffer_id Buffer that should be evaluated
 * @return 0 on success, -1 if queue is full
 */
int async_queue_push_beat(AsyncEventQueue *queue, double beat, double quantum, int buffer_id);

/**
 * Push a timer event.
 *
 * @param queue The event queue (or NULL for global)
 * @param timer_id Timer identifier
 * @param userdata User-provided data
 * @return 0 on success, -1 if queue is full
 */
int async_queue_push_timer(AsyncEventQueue *queue, int timer_id, void *userdata);

/**
 * Push a custom event with a tag.
 *
 * @param queue The event queue (or NULL for global)
 * @param tag Event tag (max 15 chars)
 * @param data Event data (will be copied if len > 0)
 * @param len Data length
 * @return 0 on success, -1 if queue is full
 */
int async_queue_push_custom(AsyncEventQueue *queue, const char *tag, const void *data, size_t len);

/* ============================================================================
 * Consumer API (Main Thread Only)
 * ============================================================================
 *
 * These functions should only be called from the main thread.
 */

/**
 * Poll for the next event without removing it.
 *
 * @param queue The event queue (or NULL for global)
 * @param event Output: the next event (if available)
 * @return 0 if event available, 1 if queue empty
 */
int async_queue_peek(AsyncEventQueue *queue, AsyncEvent *event);

/**
 * Poll and remove the next event from the queue.
 *
 * @param queue The event queue (or NULL for global)
 * @param event Output: the removed event (if available)
 * @return 0 if event available, 1 if queue empty
 */
int async_queue_poll(AsyncEventQueue *queue, AsyncEvent *event);

/**
 * Pop (discard) the next event from the queue.
 * Use this after processing an event obtained via peek.
 *
 * @param queue The event queue (or NULL for global)
 */
void async_queue_pop(AsyncEventQueue *queue);

/**
 * Check if the queue is empty.
 *
 * @param queue The event queue (or NULL for global)
 * @return 1 if empty, 0 if events pending
 */
int async_queue_is_empty(AsyncEventQueue *queue);

/**
 * Get the number of pending events.
 *
 * @param queue The event queue (or NULL for global)
 * @return Number of events in queue
 */
int async_queue_count(AsyncEventQueue *queue);

/**
 * Dispatch all pending events to registered handlers.
 *
 * @param queue The event queue (or NULL for global)
 * @param ctx Context passed to handlers
 * @return Number of events dispatched
 */
int async_queue_dispatch_all(AsyncEventQueue *queue, void *ctx);

/**
 * Dispatch events with Lua state for callbacks.
 * This is the primary dispatch function for the editor main loop.
 *
 * @param queue The event queue (or NULL for global)
 * @param ctx Editor context
 * @param L Lua state for callback invocation
 * @return Number of events dispatched
 */
int async_queue_dispatch_lua(AsyncEventQueue *queue, editor_ctx_t *ctx, struct lua_State *L);

/* ============================================================================
 * Handler Registration
 * ============================================================================ */

/**
 * Set a handler for a specific event type.
 *
 * @param queue The event queue (or NULL for global)
 * @param type Event type to handle
 * @param handler Handler function (NULL to unregister)
 */
void async_queue_set_handler(AsyncEventQueue *queue, AsyncEventType type, AsyncEventHandler handler);

/**
 * Get the current handler for an event type.
 *
 * @param queue The event queue (or NULL for global)
 * @param type Event type
 * @return Current handler or NULL if not set
 */
AsyncEventHandler async_queue_get_handler(AsyncEventQueue *queue, AsyncEventType type);

/* ============================================================================
 * Default Handlers
 * ============================================================================
 *
 * These can be registered to provide standard behavior for common events.
 */

/**
 * Default handler for ASYNC_EVENT_LANG_CALLBACK.
 * Invokes the registered Lua callback for the completed playback slot.
 */
void async_handler_lang_callback(AsyncEvent *event, void *ctx);

/**
 * Default handler for ASYNC_EVENT_LINK_PEERS.
 * Invokes the registered Lua callback for peer count changes.
 */
void async_handler_link_peers(AsyncEvent *event, void *ctx);

/**
 * Default handler for ASYNC_EVENT_LINK_TEMPO.
 * Invokes the registered Lua callback for tempo changes.
 */
void async_handler_link_tempo(AsyncEvent *event, void *ctx);

/**
 * Default handler for ASYNC_EVENT_LINK_TRANSPORT.
 * Invokes the registered Lua callback for transport changes.
 */
void async_handler_link_transport(AsyncEvent *event, void *ctx);

/**
 * Default handler for ASYNC_EVENT_BEAT_BOUNDARY.
 * Evaluates the buffer for live loop re-triggering.
 */
void async_handler_beat_boundary(AsyncEvent *event, void *ctx);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * Free any heap-allocated data in an event.
 * Called automatically by async_queue_pop/poll, but can be called manually.
 *
 * @param event The event to clean up
 */
void async_event_cleanup(AsyncEvent *event);

/**
 * Get the name of an event type (for debugging).
 *
 * @param type Event type
 * @return String name of the event type
 */
const char *async_event_type_name(AsyncEventType type);

#endif /* LOKI_ASYNC_QUEUE_H */
