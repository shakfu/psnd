/* session.h - Opaque editor session API
 *
 * This module provides a clean, opaque API for embedding the editor.
 * All internal implementation details are hidden behind the EditorSession handle.
 *
 * Usage:
 *   EditorConfig config = { .rows = 24, .cols = 80 };
 *   EditorSession *session = editor_session_new(&config);
 *
 *   // Process input
 *   EditorEvent ev = event_from_keycode('i');
 *   editor_session_handle_event(session, &ev);
 *
 *   // Get render state
 *   EditorViewModel *vm = editor_session_snapshot(session);
 *   // ... render using vm ...
 *   editor_viewmodel_free(vm);
 *
 *   editor_session_free(session);
 */

#ifndef LOKI_SESSION_H
#define LOKI_SESSION_H

#include <stddef.h>
#include "loki/core.h"  /* EditorMode */
#include "event.h"
#include "renderer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ======================= Opaque Handle ===================================== */

/**
 * EditorSession - Opaque handle to an editor instance.
 *
 * Encapsulates all editor state including:
 * - Buffer content and metadata
 * - Cursor position and viewport
 * - Modal state (normal/insert/visual/command)
 * - Undo/redo history
 * - Syntax highlighting
 */
typedef struct EditorSession EditorSession;

/* ======================= Configuration ===================================== */

/**
 * EditorConfig - Configuration for creating an editor session.
 *
 * All fields have sensible defaults (zero-initialized struct is valid).
 */
typedef struct {
    int rows;               /* Screen height (default: 24) */
    int cols;               /* Screen width (default: 80) */
    const char *filename;   /* Initial file to open (NULL for empty buffer) */
    int line_numbers;       /* Show line numbers (default: 0) */
    int word_wrap;          /* Enable word wrap (default: 0) */
    int enable_lua;         /* Enable Lua scripting (default: 0) */
    int undo_limit;         /* Max undo operations (default: 1000, 0 to disable) */
    size_t undo_memory;     /* Max undo memory bytes (default: 10MB) */

    /* Audio backend for the file's language (NULL for the default) */
    const char *soundfont_path; /* TinySoundFont soundfont (-sf) */
    const char *csound_path;    /* Csound CSD file (-cs) */
    const char *plugin_path;    /* VST3/AU plugin (--plugin) */
    const char *plugin_log;     /* Plugin debug log (--plugin-log, NULL to suppress) */

    /* OSC (Open Sound Control) configuration */
    int osc_enabled;        /* Enable OSC server (default: 0) */
    int osc_port;           /* OSC server port (default: 7770, 0 for default) */
    const char *osc_send_host;  /* Broadcast host (NULL to disable) */
    const char *osc_send_port;  /* Broadcast port (required if host is set) */
} EditorConfig;

/* ======================= View Model ======================================== */

/**
 * EditorRowView - Render data for a single row.
 *
 * Contains owned copies of all data needed to render one row.
 * The segments array contains text spans with styling information.
 */
typedef struct {
    int row_num;            /* Row number (1-based, 0 for empty rows past EOF) */
    int is_empty;           /* True if this is an empty row (past end of file) */
    RenderSegment *segments;/* Array of render segments (owned) */
    int segment_count;      /* Number of segments */
    char *text;             /* Backing storage for segment text (owned) */
} EditorRowView;

/**
 * EditorCursor - Cursor position information.
 */
typedef struct {
    int row;                /* Cursor row (1-based, screen coordinates) */
    int col;                /* Cursor column (1-based, screen coordinates) */
    int file_row;           /* Cursor row in file (0-based) */
    int file_col;           /* Cursor column in file (0-based) */
    int visible;            /* Cursor should be visible */
} EditorCursor;

/**
 * EditorTabInfo - Tab bar information.
 */
typedef struct {
    char **labels;          /* Array of tab labels (owned) */
    int count;              /* Number of tabs */
    int active;             /* Index of active tab */
} EditorTabInfo;

/**
 * EditorViewModel - Complete render state snapshot.
 *
 * Contains all data needed to render the editor UI.
 * All data is owned by the view model and must be freed with editor_viewmodel_free().
 *
 * This is a deep copy of the editor state at a point in time, safe to use
 * from any thread and survives subsequent editor mutations.
 */
typedef struct {
    /* Screen dimensions */
    int rows;               /* Total screen rows */
    int cols;               /* Total screen columns */

    /* Row content */
    EditorRowView *row_views;   /* Array of row views (owned) */
    int row_count;              /* Number of row views */
    int gutter_width;           /* Width of line number gutter (0 if disabled) */

    /* Tab bar */
    EditorTabInfo tabs;         /* Tab information (owned) */

    /* Status bar */
    StatusInfo status;          /* Status bar info (strings are owned copies) */
    char *status_mode;          /* Owned copy of mode string */
    char *status_filename;      /* Owned copy of filename */
    char *status_lang;          /* Owned copy of language indicator */

    /* Message line */
    char *message;              /* Status message (owned, NULL if none) */

    /* REPL pane */
    int repl_active;            /* REPL is visible */
    ReplInfo repl;              /* REPL info (if active) */
    char *repl_prompt;          /* Owned copy of prompt */
    char *repl_input;           /* Owned copy of input */
    char **repl_log;            /* Owned copies of log lines */
    int repl_log_count;         /* Number of log lines */

    /* Cursor */
    EditorCursor cursor;        /* Cursor position */
} EditorViewModel;

/* ======================= Session Lifecycle ================================= */

/**
 * Create a new editor session.
 *
 * @param config  Configuration (may be NULL for defaults)
 * @return New session handle, or NULL on error
 */
EditorSession *editor_session_new(const EditorConfig *config);

/**
 * Free an editor session and all associated resources.
 *
 * @param session  Session to free (may be NULL)
 */
void editor_session_free(EditorSession *session);

/* ======================= Event Handling ==================================== */

/**
 * Process an input event.
 *
 * @param session  Editor session
 * @param event    Event to process
 * @return 0 on success, 1 if editor should quit, -1 on error
 */
int editor_session_handle_event(EditorSession *session, const EditorEvent *event);

/* ======================= View Model ======================================== */

/**
 * Get a snapshot of the current render state.
 *
 * Returns a deep copy of all data needed to render the editor.
 * The caller owns the returned view model and must free it with editor_viewmodel_free().
 *
 * @param session  Editor session
 * @return New view model, or NULL on error
 */
EditorViewModel *editor_session_snapshot(EditorSession *session);

/**
 * Free a view model and all owned data.
 *
 * @param vm  View model to free (may be NULL)
 */
void editor_viewmodel_free(EditorViewModel *vm);

/* ======================= Convenience Accessors ============================= */

/**
 * Get the current editor mode.
 *
 * @param session  Editor session
 * @return Current mode (MODE_NORMAL, MODE_INSERT, etc.)
 */
EditorMode editor_session_get_mode(EditorSession *session);

/**
 * Check if the buffer has unsaved changes.
 *
 * @param session  Editor session
 * @return 1 if modified, 0 if not
 */
int editor_session_is_dirty(EditorSession *session);

/**
 * Get the current filename.
 *
 * @param session  Editor session
 * @return Filename (internal pointer, do not free), or NULL if no file
 */
const char *editor_session_get_filename(EditorSession *session);

/**
 * Set screen dimensions.
 *
 * Call this when the display size changes.
 *
 * @param session  Editor session
 * @param rows     New screen height
 * @param cols     New screen width
 */
void editor_session_resize(EditorSession *session, int rows, int cols);

/**
 * Open a file in the editor.
 *
 * @param session   Editor session
 * @param filename  File to open
 * @return 0 on success, -1 on error
 */
int editor_session_open(EditorSession *session, const char *filename);

/**
 * Save the current buffer.
 *
 * @param session  Editor session
 * @return 0 on success, -1 on error
 */
int editor_session_save(EditorSession *session);

/**
 * Get the editor context from a session.
 *
 * This provides direct access to the internal editor context for advanced
 * operations like language evaluation. Use with caution.
 *
 * @param session  Editor session
 * @return Editor context pointer, or NULL on error
 */
struct editor_ctx *editor_session_get_ctx(EditorSession *session);

#ifdef __cplusplus
}
#endif

#endif /* LOKI_SESSION_H */
