/* loki_internal.h - Internal structures and function declarations
 *
 * This header contains internal structures and function declarations needed
 * by both loki_core.c and loki_lua.c. It is NOT part of the public API and
 * should only be used within the loki library implementation.
 */

#ifndef LOKI_INTERNAL_H
#define LOKI_INTERNAL_H

#include <stddef.h>
#include <time.h>
#include <signal.h>
#include <lua.h>
#include "loki/core.h"  /* Public API types (EditorMode, etc.) */
#include "event.h"      /* Event abstraction layer */
#include "renderer.h"   /* Renderer abstraction layer */
#include "hl_types.h"   /* Syntax highlighting type constants */

/* ======================= Key Constants =================================== */

enum KEY_ACTION{
        KEY_NULL = 0,       /* NULL */
        CTRL_C = 3,         /* Ctrl-c */
        CTRL_D = 4,         /* Ctrl-d */
        CTRL_E = 5,         /* Ctrl-e (eval Alda) */
        CTRL_F = 6,         /* Ctrl-f */
        CTRL_G = 7,         /* Ctrl-g (stop Alda) */
        CTRL_H = 8,         /* Ctrl-h */
        TAB = 9,            /* Tab */
        CTRL_L = 12,        /* Ctrl+l */
        ENTER = 13,         /* Enter */
        CTRL_P = 16,        /* Ctrl-p (play file) */
        CTRL_Q = 17,        /* Ctrl-q */
        CTRL_S = 19,        /* Ctrl-s */
        CTRL_T = 20,        /* Ctrl-t */
        CTRL_U = 21,        /* Ctrl-u */
        CTRL_W = 23,        /* Ctrl-w */
        CTRL_X = 24,        /* Ctrl-x */
        ESC = 27,           /* Escape */
        BACKSPACE =  127,   /* Backspace */
        /* The following are just soft codes, not really reported by the
         * terminal directly. */
        ARROW_LEFT = 1000,
        ARROW_RIGHT,
        ARROW_UP,
        ARROW_DOWN,
        SHIFT_ARROW_LEFT,
        SHIFT_ARROW_RIGHT,
        SHIFT_ARROW_UP,
        SHIFT_ARROW_DOWN,
        DEL_KEY,
        HOME_KEY,
        END_KEY,
        PAGE_UP,
        PAGE_DOWN,
        SHIFT_RETURN
};

/* ======================= Configuration Constants ========================== */

#define KILO_QUERY_LEN 256
#define STATUS_ROWS 2

#define LUA_REPL_HISTORY_MAX 64
#define LUA_REPL_LOG_MAX 128
#define LUA_REPL_OUTPUT_ROWS 2
#define LUA_REPL_TOTAL_ROWS (LUA_REPL_OUTPUT_ROWS + 1)
#define LUA_REPL_PROMPT ">> "

/* ======================= Forward Declarations ============================= */

/* Undo/redo state - opaque pointer, defined in loki_undo.c */
struct undo_state;

/* Shared audio/MIDI/Link context - see shared/context.h */
struct SharedContext;

/* Window manager - binary tree tiling window manager, see loki/window.h */
struct WindowManager;

/* Tree-sitter state - used when LOKI_USE_LINENOISE is defined */
#ifdef LOKI_USE_LINENOISE
struct TreeSitterState;
#endif

/* Language state forward declarations - see src/lang_config.h */
#include "../lang_config.h"

/* ======================= Data Structures ================================== */

/* Syntax highlighting color definition */
typedef struct t_hlcolor {
    int r,g,b;
} t_hlcolor;

/* Syntax highlighting rules per language */
struct t_editor_syntax {
    char **filematch;
    char **keywords;
    char singleline_comment_start[4];   /* Increased to support longer comment syntax */
    char multiline_comment_start[6];    /* Increased for Lua's --[[ */
    char multiline_comment_end[6];      /* Increased for potential longer delimiters */
    char *separators;
    int flags;
    int type;  /* HL_TYPE_* */
};

/* This structure represents a single line of the file we are editing. */
typedef struct t_erow {
    int idx;            /* Row index in the file, zero-based. */
    int size;           /* Size of the row, excluding the null term. */
    int rsize;          /* Size of the rendered row. */
    char *chars;        /* Row content. */
    char *render;       /* Row content "rendered" for screen (for TABs). */
    unsigned char *hl;  /* Syntax highlight type for each character in render.*/
    int hl_oc;          /* Row had open comment at end in last syntax highlight
                           check. */
    int cb_lang;        /* Code block language (for markdown): CB_LANG_* */
    int csd_section;    /* CSD section (for Csound): CSD_SECTION_* */
} t_erow;

/* Lua REPL state */
typedef struct t_lua_repl {
    char input[KILO_QUERY_LEN+1];
    int input_len;
    int active;
    int history_len;
    int history_index;
    char *history[LUA_REPL_HISTORY_MAX];
    int log_len;
    char *log[LUA_REPL_LOG_MAX];
} t_lua_repl;

/* Lua host */
typedef struct LuaHost {
    lua_State *L;
    t_lua_repl repl;
} LuaHost;

/* ======================= Model/View Separation ============================= */

/* EditorModel - Document state that persists across views.
 * Contains buffer content, file metadata, and language-specific state.
 * Multiple views can share the same model in future implementations. */
typedef struct EditorModel {
    t_erow *row;              /* Buffer content (rows) */
    int numrows;              /* Number of rows */
    char *filename;           /* Currently open filename */
    int dirty;                /* File modified but not saved */
    struct undo_state *undo_state;        /* Undo/redo state (NULL if disabled) */
    struct indent_config *indent_config;  /* Auto-indent settings */

#ifdef LOKI_USE_LINENOISE
    struct TreeSitterState *ts_state;     /* Tree-sitter syntax highlighting */
#endif

    /* Shared audio/MIDI/Link context - single instance for all languages.
     * Owned by the editor, shared by all language subsystems.
     * See shared/context.h for details. */
    struct SharedContext *shared;

    /* Language states - per-context state (NULL until initialized)
     * See lang_config.h to add new languages */
    LOKI_LANG_STATE_FIELDS
} EditorModel;

/* EditorView - Presentation state that is terminal/viewport-specific.
 * Contains cursor position, viewport offset, display settings, and UI state.
 * Each view has its own cursor, scroll position, and mode. */
typedef struct EditorView {
    /* Cursor position */
    int cx, cy;               /* Cursor x and y position in characters */

    /* Viewport offset */
    int rowoff;               /* Offset of row displayed */
    int coloff;               /* Offset of column displayed */

    /* Screen dimensions */
    int screenrows;           /* Number of rows that we can show */
    int screencols;           /* Number of cols that we can show */
    int screenrows_total;     /* Rows available after status bars (before REPL) */

    /* Selection state */
    int sel_active;           /* Selection active flag */
    int sel_start_x, sel_start_y;  /* Selection start position */
    int sel_end_x, sel_end_y;      /* Selection end position */

    /* Display settings */
    struct t_editor_syntax *syntax;  /* Current syntax highlight, or NULL */
    t_hlcolor colors[HL_TYPE_COUNT];  /* Syntax highlight colors: indexed by HL_* constants */
    int line_numbers;         /* Line numbers display flag */
    int word_wrap;            /* Word wrap enabled flag */

    /* Modal state */
    EditorMode mode;          /* Current editor mode (normal/insert/visual/command) */
    char cmd_buffer[256];     /* Command input buffer */
    int cmd_length;           /* Length of command */
    int cmd_cursor_pos;       /* Cursor position in command */
    int cmd_history_index;    /* Current history position */
    int pending_prefix;       /* Pending prefix key (e.g., CTRL_X for Ctrl-X sequences), 0 if none */
    int quit_times;           /* Remaining Ctrl-Q presses needed to quit a dirty buffer */

    /* Status */
    char statusmsg[80];       /* Status message */
    time_t statusmsg_time;    /* Status message timestamp */

    /* Picker state (for MODE_PICKER) */
    struct {
        const char **items;         /* Item strings (not owned) */
        int item_count;
        int selected_index;
        int scroll_offset;
        const char *title;
        void (*on_select)(editor_ctx_t *ctx, int index, void *data);
        void (*on_cancel)(editor_ctx_t *ctx);
        void *user_data;
        EditorMode prev_mode;       /* Mode to restore on close */
    } picker;

    /* Playback visualization state */
    int playing_line;               /* Currently playing source line (1-based, 0=none) */
} EditorView;

/* Editor context - one instance per editor viewport/buffer.
 * This structure will enable multiple independent editor contexts for future
 * split windows and multiple buffers implementation.
 * The typedef editor_ctx_t is declared in loki/core.h (public API).
 *
 * ARCHITECTURE: Model/View separation enables:
 * - Multiple views of the same document (split windows)
 * - Independent cursor/scroll positions per view
 * - Future serialization of model state
 * - Potential RPC/remote editing scenarios */
struct editor_ctx {
    EditorModel model;        /* Document state (buffer, file, undo, languages) */
    EditorView view;          /* Presentation state (cursor, viewport, UI) */
    LuaHost *lua_host;        /* Lua host */
    Renderer *renderer;       /* Rendering abstraction (may be NULL for direct VT100) */
    struct WindowManager *wm; /* Window manager for split windows (NULL if single pane) */
};

/* ======================= Compatibility Macros ============================== */
/* These macros provide backwards compatibility during the migration period.
 * They allow existing code to continue using ctx->field syntax while we
 * incrementally migrate to ctx->model.field or ctx->view.field.
 * REMOVE these macros once migration is complete. */

/* Model field accessors */
#define ctx_row(ctx) ((ctx)->model.row)
#define ctx_numrows(ctx) ((ctx)->model.numrows)
#define ctx_filename(ctx) ((ctx)->model.filename)
#define ctx_dirty(ctx) ((ctx)->model.dirty)
#define ctx_undo_state(ctx) ((ctx)->model.undo_state)
#define ctx_indent_config(ctx) ((ctx)->model.indent_config)

/* View field accessors */
#define ctx_cx(ctx) ((ctx)->view.cx)
#define ctx_cy(ctx) ((ctx)->view.cy)
#define ctx_rowoff(ctx) ((ctx)->view.rowoff)
#define ctx_coloff(ctx) ((ctx)->view.coloff)
#define ctx_screenrows(ctx) ((ctx)->view.screenrows)
#define ctx_screencols(ctx) ((ctx)->view.screencols)
#define ctx_screenrows_total(ctx) ((ctx)->view.screenrows_total)
#define ctx_sel_active(ctx) ((ctx)->view.sel_active)
#define ctx_sel_start_x(ctx) ((ctx)->view.sel_start_x)
#define ctx_sel_start_y(ctx) ((ctx)->view.sel_start_y)
#define ctx_sel_end_x(ctx) ((ctx)->view.sel_end_x)
#define ctx_sel_end_y(ctx) ((ctx)->view.sel_end_y)
#define ctx_syntax(ctx) ((ctx)->view.syntax)
#define ctx_colors(ctx) ((ctx)->view.colors)
#define ctx_line_numbers(ctx) ((ctx)->view.line_numbers)
#define ctx_word_wrap(ctx) ((ctx)->view.word_wrap)
#define ctx_mode(ctx) ((ctx)->view.mode)
#define ctx_cmd_buffer(ctx) ((ctx)->view.cmd_buffer)
#define ctx_cmd_length(ctx) ((ctx)->view.cmd_length)
#define ctx_cmd_cursor_pos(ctx) ((ctx)->view.cmd_cursor_pos)
#define ctx_cmd_history_index(ctx) ((ctx)->view.cmd_history_index)
#define ctx_repl(ctx) ((ctx)->lua_host ? &(ctx)->lua_host->repl : NULL)
#define ctx_statusmsg(ctx) ((ctx)->view.statusmsg)
#define ctx_statusmsg_time(ctx) ((ctx)->view.statusmsg_time)
#define ctx_L(ctx) ((ctx)->lua_host ? (ctx)->lua_host->L : NULL)

/* Legacy type name for compatibility during migration.
 * New code should use editor_ctx_t. */
typedef editor_ctx_t loki_editor_instance;

/* ======================= Screen Buffer =================================== */

/* Append buffer for building screen output */
struct abuf {
    char *b;
    int len;
};

#define ABUF_INIT {NULL,0}

/* Screen buffer functions are now in loki_terminal.h */

/* ======================= Function Declarations ============================ */

/* Context management (for future split windows and multi-buffer support) */
void editor_ctx_init(editor_ctx_t *ctx);
void editor_ctx_free(editor_ctx_t *ctx);
void editor_ctx_set_renderer(editor_ctx_t *ctx, Renderer *renderer);

/* Status message */
void editor_set_status_msg(editor_ctx_t *ctx, const char *fmt, ...);

/* VST3/AU plugin (--plugin); reports in the status bar, returns 0 on success */
int editor_load_plugin(editor_ctx_t *ctx, const char *path, const char *log_path);

/* Character insertion (context-aware) */
void editor_insert_char(editor_ctx_t *ctx, int c);
void editor_insert_newline(editor_ctx_t *ctx);
void editor_del_char(editor_ctx_t *ctx);

/* Run the Lua loki.highlight_row hook on a freshly highlighted row. */
void lua_apply_highlight_row(editor_ctx_t *ctx, t_erow *row, int default_ran);

/* Ctrl-Q presses required to abandon a modified buffer. */
#define KILO_QUIT_TIMES 3

/* Row management */
void editor_insert_row(editor_ctx_t *ctx, int at, char *s, size_t len);
void editor_del_row(editor_ctx_t *ctx, int at);
void editor_row_insert_char(editor_ctx_t *ctx, t_erow *row, int at, int c);
void editor_row_del_char(editor_ctx_t *ctx, t_erow *row, int at);
void editor_row_append_string(editor_ctx_t *ctx, t_erow *row, char *s, size_t len);

/* Write that deliberately ignores failure (terminal escape sequences: there
 * is nothing useful to do if the terminal has gone away). */
#ifdef _WIN32
#include <io.h>
static inline void write_ignore_result(int fd, const void *buf, size_t n) {
    int r = _write(fd, buf, (unsigned int)n);
    (void)r;
}
#else
#include <unistd.h>
static inline void write_ignore_result(int fd, const void *buf, size_t n) {
    ssize_t r = write(fd, buf, n);
    (void)r;
}
#endif

/* Screen rendering */
void editor_refresh_screen(editor_ctx_t *ctx);

/* Lua REPL functions */
void lua_repl_init(t_lua_repl *repl);
void lua_repl_free(t_lua_repl *repl);

/* LuaHost lifecycle functions */
LuaHost *lua_host_create(void);
void lua_host_free(LuaHost *host);
void lua_host_init_repl(LuaHost *host);
void lua_repl_handle_keypress(editor_ctx_t *ctx, int key);
void lua_repl_render(editor_ctx_t *ctx, struct abuf *ab);
void lua_repl_append_log(editor_ctx_t *ctx, const char *line);
void editor_update_repl_layout(editor_ctx_t *ctx);

/* Editor cleanup */
void editor_cleanup_resources(editor_ctx_t *ctx);

/* Syntax highlighting helpers */
int hl_name_to_code(const char *name);
int is_separator(int c, char *separators);

/* Terminal and input functions are now in loki_terminal.h */
int editor_process_keypress(editor_ctx_t *ctx, int fd);

/* Cursor movement */
void editor_move_cursor(editor_ctx_t *ctx, int key);

/* Modal editing - event-based entry point
 * Process an EditorEvent through the modal system.
 * Preferred for testing and non-terminal input sources.
 * Note: Does not support Ctrl-X prefix sequences (use modal_process_keypress). */
void modal_process_event(editor_ctx_t *ctx, const EditorEvent *event);

/* Modal editing test functions - for testing only
 * These functions expose the internal mode handlers for unit testing.
 * They should not be used in production code. */
void modal_process_normal_mode_key(editor_ctx_t *ctx, int fd, int c);
void modal_process_insert_mode_key(editor_ctx_t *ctx, int fd, int c);
void modal_process_visual_mode_key(editor_ctx_t *ctx, int fd, int c);

/* Syntax highlighting test functions - for testing only
 * These functions expose internal syntax highlighting for unit testing.
 * They should not be used in production code. */
void editor_update_syntax(editor_ctx_t *ctx, t_erow *row);
void editor_update_row(editor_ctx_t *ctx, t_erow *row);

/* Search test functions - for testing only
 * These functions expose internal search logic for unit testing.
 * They should not be used in production code. */
int editor_find_next_match(editor_ctx_t *ctx, const char *query, int start_row, int direction, int *match_offset);

#endif /* LOKI_INTERNAL_H */
