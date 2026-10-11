/* loki_event.h - Abstract input event layer
 *
 * This module provides a structured event abstraction for editor input:
 * - Replaces raw keycodes with EditorEvent objects
 * - Clean modifier handling (Ctrl/Shift/Alt as flags)
 * - Enables test injection without terminal I/O
 * - Supports future transports (WebSocket, RPC)
 * - Extensible for mouse, resize, and custom actions
 *
 * The event layer sits between terminal input and modal processing,
 * converting raw keycodes into structured events while maintaining
 * backward compatibility with existing code.
 */

#ifndef LOKI_EVENT_H
#define LOKI_EVENT_H

#include <stdint.h>
#include <stddef.h>

/* Undefine any conflicting Windows macros (winuser.h defines MOD_* for hotkeys) */
#ifdef _WIN32
#undef MOD_SHIFT
#undef MOD_CONTROL
#undef MOD_ALT
#undef MOD_WIN
#endif

/* ======================= Event Types ====================================== */

typedef enum {
    EVENT_NONE = 0,
    EVENT_KEY,        /* Keyboard input */
    EVENT_COMMAND,    /* Ex-command string (e.g., ":w", ":q") */
    EVENT_ACTION,     /* Named action (e.g., "save", "quit") */
    EVENT_RESIZE,     /* Terminal resize */
    EVENT_MOUSE,      /* Mouse input (future) */
    EVENT_QUIT,       /* Quit request */
} EditorEventType;

/* ======================= Modifier Flags =================================== */

typedef enum {
    MOD_NONE  = 0,
    MOD_CTRL  = (1 << 0),
    MOD_SHIFT = (1 << 1),
    MOD_ALT   = (1 << 2),
} EditorModifier;

/* ======================= EditorEvent Structure ============================ */

/**
 * EditorEvent - Unified input event structure
 *
 * Encapsulates all forms of editor input in a single structure.
 * The union allows type-specific data without memory overhead.
 *
 * Usage:
 *   EditorEvent ev = event_from_keycode(c);
 *   if (ev.type == EVENT_KEY) {
 *       if (ev.data.key.modifiers & MOD_CTRL) { ... }
 *   }
 */
typedef struct {
    EditorEventType type;
    union {
        /* EVENT_KEY: Keyboard input */
        struct {
            int keycode;           /* Base keycode (without modifier encoding) */
            uint8_t modifiers;     /* EditorModifier flags */
            char utf8[5];          /* UTF-8 representation (null-terminated) */
            uint8_t utf8_len;      /* Length of UTF-8 sequence */
        } key;

        /* EVENT_COMMAND: Ex-command string */
        struct {
            const char *command;   /* Command string (e.g., "w", "q!", "set nu") */
        } command;

        /* EVENT_ACTION: Named action */
        struct {
            const char *action;    /* Action name (e.g., "save", "quit") */
            const char *args;      /* Optional arguments */
        } action;

        /* EVENT_RESIZE: Terminal resize */
        struct {
            int rows;   /* Text rows, excluding the host's status lines */
            int cols;
        } resize;

        /* EVENT_MOUSE: Mouse input (future) */
        struct {
            int x, y;              /* Click position */
            int button;            /* Mouse button (0=left, 1=middle, 2=right) */
            int pressed;           /* 1=pressed, 0=released */
            uint8_t modifiers;     /* EditorModifier flags */
        } mouse;
    } data;
} EditorEvent;

/* ======================= EventSource Interface ============================ */

/**
 * EventSource - Abstract event input source
 *
 * Provides a uniform interface for reading events from different sources:
 * - Terminal (wraps terminal_read_key)
 * - Test queue (for unit testing without I/O)
 * - Future: WebSocket, RPC, replay files
 *
 * Usage:
 *   EventSource *src = event_source_terminal(STDIN_FILENO);
 *   EditorEvent ev = src->read(src, 100);  // 100ms timeout
 *   src->destroy(src);
 */
typedef struct EventSource EventSource;

struct EventSource {
    /**
     * Read next event from source.
     * @param src      Event source instance
     * @param timeout_ms  Timeout in milliseconds (-1 for blocking)
     * @return Event, or EVENT_NONE on timeout/error
     */
    EditorEvent (*read)(EventSource *src, int timeout_ms);

    /**
     * Check if events are available (non-blocking).
     * @param src  Event source instance
     * @return 1 if event available, 0 otherwise
     */
    int (*poll)(EventSource *src);

    /**
     * Destroy event source and free resources.
     * @param src  Event source instance
     */
    void (*destroy)(EventSource *src);

    /** Private data for implementation */
    void *data;
};

/* ======================= EventSource Factory Functions ==================== */

/**
 * Create terminal event source.
 * Wraps terminal_read_key() and converts keycodes to events.
 * @param fd  Terminal file descriptor (usually STDIN_FILENO)
 * @return Event source, or NULL on error
 */
EventSource *event_source_terminal(int fd);

/**
 * Create test event source.
 * Queue-based source for unit testing without terminal I/O.
 * Use event_source_test_push() to inject events.
 * @return Event source, or NULL on error
 */
EventSource *event_source_test(void);

/**
 * Push event into test source queue.
 * @param src  Test event source (must be created with event_source_test())
 * @param ev   Event to push
 * @return 0 on success, -1 on error (queue full or wrong source type)
 */
int event_source_test_push(EventSource *src, const EditorEvent *ev);

/**
 * Push keycode into test source queue (convenience wrapper).
 * Converts keycode to event and pushes.
 * @param src      Test event source
 * @param keycode  Keycode to push
 * @return 0 on success, -1 on error
 */
int event_source_test_push_key(EventSource *src, int keycode);

/* ======================= Conversion Functions ============================= */

/**
 * Convert legacy keycode to EditorEvent.
 * Decomposes modifier-encoded keycodes (e.g., SHIFT_ARROW_UP -> ARROW_UP + MOD_SHIFT).
 * @param keycode  Legacy keycode from terminal_read_key()
 * @return EditorEvent with type EVENT_KEY
 */
EditorEvent event_from_keycode(int keycode);

/**
 * Convert EditorEvent to legacy keycode.
 * Recomposes modifier-encoded keycodes for backward compatibility.
 * @param ev  EditorEvent (must be EVENT_KEY type)
 * @return Legacy keycode, or 0 for non-key events
 */
int event_to_keycode(const EditorEvent *ev);

/* ======================= Event Construction Helpers ======================= */

/**
 * Create a key event.
 */
EditorEvent event_key(int keycode, uint8_t modifiers);

/**
 * Create a command event.
 */
EditorEvent event_command(const char *command);

/**
 * Create an action event.
 */
EditorEvent event_action(const char *action, const char *args);

/**
 * Create a resize event.
 */
EditorEvent event_resize(int rows, int cols);

/**
 * Create a quit event.
 */
EditorEvent event_quit(void);

/* ======================= Event Query Functions ============================ */

/**
 * Check if event has Ctrl modifier.
 */
static inline int event_has_ctrl(const EditorEvent *ev) {
    return ev->type == EVENT_KEY && (ev->data.key.modifiers & MOD_CTRL);
}

/**
 * Check if event has Shift modifier.
 */
static inline int event_has_shift(const EditorEvent *ev) {
    return ev->type == EVENT_KEY && (ev->data.key.modifiers & MOD_SHIFT);
}

/**
 * Check if event has Alt modifier.
 */
static inline int event_has_alt(const EditorEvent *ev) {
    return ev->type == EVENT_KEY && (ev->data.key.modifiers & MOD_ALT);
}

/**
 * Check if event is a printable character (ASCII 32-126).
 */
static inline int event_is_printable(const EditorEvent *ev) {
    if (ev->type != EVENT_KEY) return 0;
    int k = ev->data.key.keycode;
    return k >= 32 && k < 127 && !(ev->data.key.modifiers & (MOD_CTRL | MOD_ALT));
}

#endif /* LOKI_EVENT_H */
