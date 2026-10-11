/* session.c - Opaque editor session implementation
 *
 * This module implements the EditorSession API, providing a clean interface
 * for embedding the editor without exposing internal implementation details.
 */

#include "session.h"
#include "internal.h"
#include "selection.h"
#include "buffers.h"
#include "terminal.h"
#include "undo.h"
#include "lang_bridge.h"
#include "loki/link.h"
#include "loki/lua.h"
#include "syntax.h"
#include "shared/context.h"
#include "shared/osc/osc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
/* Windows (MSVC and MinGW with -std=c11) doesn't have strndup */
static char* strndup(const char* s, size_t n) {
    size_t len = strnlen(s, n);
    char* result = (char*)malloc(len + 1);
    if (result) {
        memcpy(result, s, len);
        result[len] = '\0';
    }
    return result;
}
#endif

/* Maximum render segments per row */
#define MAX_SEGMENTS_PER_ROW 256

/* ======================= Internal Structure ================================ */

struct EditorSession {
    editor_ctx_t ctx;       /* Editor context (owned) */
    int initialized;        /* Initialization flag */
    int should_quit;        /* Quit flag set by event handling */
};

/* ======================= Helper Functions ================================== */

/* Duplicate a string, returns NULL if input is NULL */
static char *safe_strdup(const char *s) {
    if (!s) return NULL;
    return strdup(s);
}

/* OSC query callback: get current filename */
static const char *osc_query_get_filename(editor_ctx_t *ctx) {
    if (!ctx) return NULL;
    return ctx->model.filename;
}

/* OSC query callback: get cursor position */
static void osc_query_get_position(editor_ctx_t *ctx, int *line, int *col) {
    if (!ctx || !line || !col) return;
    *line = ctx->view.cy;
    *col = ctx->view.cx;
}

/* Build render segments for a row (same logic as in core.c) */
static int build_row_segments(editor_ctx_t *ctx, t_erow *row, int row_idx,
                              int coloff, int max_cols,
                              RenderSegment *segments) {
    if (!row || row->rsize <= coloff) return 0;

    int len = row->rsize - coloff;
    if (len > max_cols) len = max_cols;
    if (len <= 0) return 0;

    char *c = row->render + coloff;
    unsigned char *hl = row->hl + coloff;
    int seg_count = 0;

    int seg_start = 0;
    int current_hl = hl[0];
    int current_selected = is_selected(ctx, row_idx, coloff);

    for (int j = 1; j <= len && seg_count < MAX_SEGMENTS_PER_ROW - 1; j++) {
        int next_hl = (j < len) ? hl[j] : -1;
        int next_selected = (j < len) ? is_selected(ctx, row_idx, coloff + j) : 0;

        if (j == len || next_hl != current_hl || next_selected != current_selected) {
            segments[seg_count].text = c + seg_start;
            segments[seg_count].len = j - seg_start;
            segments[seg_count].hl_type = hl_const_to_type(current_hl);
            segments[seg_count].selected = current_selected;
            seg_count++;
            seg_start = j;
            current_hl = next_hl;
            current_selected = next_selected;
        }
    }

    return seg_count;
}

/* Deep copy row view with owned segment data */
static int copy_row_view(EditorRowView *dest, editor_ctx_t *ctx, int y,
                         int gutter_width, int text_cols) {
    (void)gutter_width;  /* Reserved for future gutter rendering */
    int filerow = ctx->view.rowoff + y;
    dest->is_empty = (filerow >= ctx->model.numrows);
    dest->row_num = dest->is_empty ? 0 : filerow + 1;
    dest->segments = NULL;
    dest->segment_count = 0;
    dest->text = NULL;

    if (dest->is_empty) {
        return 0;
    }

    t_erow *row = &ctx->model.row[filerow];

    /* Build segments into temp array */
    RenderSegment temp_segs[MAX_SEGMENTS_PER_ROW];
    int seg_count = build_row_segments(ctx, row, filerow,
                                       ctx->view.coloff, text_cols, temp_segs);

    if (seg_count == 0) {
        return 0;
    }

    /* Calculate total text length needed */
    size_t total_len = 0;
    for (int i = 0; i < seg_count; i++) {
        total_len += temp_segs[i].len;
    }

    /* Allocate backing text storage */
    dest->text = malloc(total_len + 1);
    if (!dest->text) return -1;

    /* Allocate segments array */
    dest->segments = malloc(seg_count * sizeof(RenderSegment));
    if (!dest->segments) {
        free(dest->text);
        dest->text = NULL;
        return -1;
    }

    /* Copy text and update segment pointers */
    char *text_ptr = dest->text;
    for (int i = 0; i < seg_count; i++) {
        memcpy(text_ptr, temp_segs[i].text, temp_segs[i].len);
        dest->segments[i].text = text_ptr;
        dest->segments[i].len = temp_segs[i].len;
        dest->segments[i].hl_type = temp_segs[i].hl_type;
        dest->segments[i].selected = temp_segs[i].selected;
        text_ptr += temp_segs[i].len;
    }
    *text_ptr = '\0';
    dest->segment_count = seg_count;

    return 0;
}

/* Free row view resources */
static void free_row_view(EditorRowView *rv) {
    free(rv->segments);
    free(rv->text);
    rv->segments = NULL;
    rv->text = NULL;
    rv->segment_count = 0;
}

/* ======================= Session Lifecycle ================================= */

/* Apply -sf / -cs to the file's language. Only reports; a failed backend
 * leaves the language on its default output. */
static void session_configure_backend(editor_ctx_t *ctx, const EditorConfig *config) {
    const char *path = config->csound_path ? config->csound_path : config->soundfont_path;
    if (!path) return;
    if (loki_lang_configure_backend(ctx, config->soundfont_path, config->csound_path) != 0) {
        const char *err = loki_lang_get_error(ctx);
        editor_set_status_msg(ctx, "Failed to load %s: %s", path, err ? err : "unknown error");
    } else {
        editor_set_status_msg(ctx, "Using %s", path);
    }
}

EditorSession *editor_session_new(const EditorConfig *config) {
    EditorSession *session = calloc(1, sizeof(EditorSession));
    if (!session) return NULL;

    /* Initialize editor context */
    editor_ctx_init(&session->ctx);

    /* Apply configuration */
    if (config) {
        session->ctx.view.screenrows = config->rows > 0 ? config->rows : 24;
        session->ctx.view.screencols = config->cols > 0 ? config->cols : 80;
        session->ctx.view.line_numbers = config->line_numbers;
        session->ctx.view.word_wrap = config->word_wrap;

        /* Reconfigure undo if specified */
        if (config->undo_limit == 0) {
            undo_free(&session->ctx);
        }
        /* Note: undo_init already called by editor_ctx_init with defaults */

        /* Open initial file if specified */
        if (config->filename) {
            syntax_select_for_filename(&session->ctx, config->filename);
            editor_open(&session->ctx, config->filename);
        }

        /* Create SharedContext for audio/MIDI/Link state (shared by all languages) */
        if (!session->ctx.model.shared) {
            session->ctx.model.shared = (SharedContext *)malloc(sizeof(SharedContext));
            if (session->ctx.model.shared) {
                if (shared_context_init(session->ctx.model.shared) != 0) {
                    fprintf(stderr, "Warning: Failed to initialize shared context\n");
                    free(session->ctx.model.shared);
                    session->ctx.model.shared = NULL;
                }
            } else {
                fprintf(stderr, "Warning: Failed to allocate shared context\n");
            }
        }

        /* Initialize OSC if requested */
        if (config->osc_enabled && session->ctx.model.shared) {
            int osc_port = config->osc_port > 0 ? config->osc_port : PSND_OSC_DEFAULT_PORT;
            if (shared_osc_init(session->ctx.model.shared, osc_port) == 0) {
                /* Set broadcast target if specified */
                if (config->osc_send_host && config->osc_send_port) {
                    shared_osc_set_broadcast(session->ctx.model.shared,
                                             config->osc_send_host,
                                             config->osc_send_port);
                }
                /* Store session as user data for OSC handlers */
                shared_osc_set_user_data(session->ctx.model.shared, session);
                /* Set up language callbacks for OSC play/eval/stop handlers */
                shared_osc_set_lang_callbacks(loki_lang_eval, loki_lang_eval_buffer, loki_lang_stop_all);
                /* Set up query callbacks for OSC query/reply handlers */
                shared_osc_set_query_callbacks(loki_lang_is_playing, osc_query_get_filename, osc_query_get_position);
                /* Start the OSC server thread */
                if (shared_osc_start(session->ctx.model.shared) == 0) {
                    fprintf(stderr, "OSC server listening on port %d\n", osc_port);
                } else {
                    fprintf(stderr, "Warning: Failed to start OSC server\n");
                }
            } else {
                fprintf(stderr, "Warning: Failed to initialize OSC on port %d\n", osc_port);
            }
        }

        /* Load VST3/AU plugin before the language, as the terminal editor does */
        if (config->plugin_path) {
            editor_load_plugin(&session->ctx, config->plugin_path, config->plugin_log);
        }

        /* Initialize Lua if requested */
        if (config->enable_lua) {
            LuaHost *lua_host = lua_host_create();
            if (lua_host) {
                session->ctx.lua_host = lua_host;

                struct loki_lua_opts opts = {
                    .bind_editor = 1,
                    .bind_http = 0,
                    .load_config = 1,
                    .config_override = NULL,
                    .project_root = NULL,
                    .extra_lua_path = NULL,
                    .reporter = NULL,
                    .reporter_userdata = NULL
                };

                lua_host->L = loki_lua_bootstrap(&session->ctx, &opts);
                if (lua_host->L) {
                    lua_host_init_repl(lua_host);

                    /* Initialize language for file type, or default language if no file */
                    if (loki_lang_init_for_file(&session->ctx) == 0) {
                        session_configure_backend(&session->ctx, config);
                    } else {
                        /* No file or unrecognized extension - try to init first available language */
                        int lang_count = 0;
                        const LokiLangOps **langs = loki_lang_all(&lang_count);
                        for (int i = 0; i < lang_count; i++) {
                            if (langs[i]->init) {
                                if (langs[i]->init(&session->ctx) == 0) {
                                    break;  /* Successfully initialized */
                                }
                            }
                        }
                    }
                }
            }
        }
    } else {
        session->ctx.view.screenrows = 24;
        session->ctx.view.screencols = 80;
    }

    session->initialized = 1;
    session->should_quit = 0;

    return session;
}

void editor_session_free(EditorSession *session) {
    if (!session) return;

    /* Clean up Lua host if we created one */
    if (session->ctx.lua_host) {
        lua_host_free(session->ctx.lua_host);
        session->ctx.lua_host = NULL;
    }

    /* Clean up shared context if we created one */
    if (session->ctx.model.shared) {
        shared_context_cleanup(session->ctx.model.shared);
        free(session->ctx.model.shared);
        session->ctx.model.shared = NULL;
    }

    editor_ctx_free(&session->ctx);
    free(session);
}

/* ======================= Event Handling ==================================== */

int editor_session_handle_event(EditorSession *session, const EditorEvent *event) {
    if (!session || !event) return -1;

    /* Check for quit event */
    if (event->type == EVENT_QUIT) {
        session->should_quit = 1;
        return 1;
    }

    /* Handle resize event */
    if (event->type == EVENT_RESIZE) {
        session->ctx.view.screenrows = event->data.resize.rows;
        session->ctx.view.screencols = event->data.resize.cols;
        return 0;
    }

    /* Process through modal system */
    modal_process_event(&session->ctx, event);

    /* Check if quit was requested */
    if (session->should_quit) {
        return 1;
    }

    return 0;
}

/* ======================= View Model ======================================== */

EditorViewModel *editor_session_snapshot(EditorSession *session) {
    if (!session) return NULL;

    editor_ctx_t *ctx = &session->ctx;
    EditorViewModel *vm = calloc(1, sizeof(EditorViewModel));
    if (!vm) return NULL;

    /* Screen dimensions */
    vm->rows = ctx->view.screenrows;
    vm->cols = ctx->view.screencols;

    /* Calculate gutter width */
    vm->gutter_width = 0;
    if (ctx->view.line_numbers && ctx->model.numrows > 0) {
        int max_line = ctx->model.numrows;
        vm->gutter_width = 1;
        while (max_line >= 10) {
            vm->gutter_width++;
            max_line /= 10;
        }
        vm->gutter_width += 1; /* Space separator */
    }

    int text_cols = vm->cols - vm->gutter_width;
    if (text_cols < 1) text_cols = 1;

    /* Tab bar */
    int tabs_showing = (buffer_count() > 1) ? 1 : 0;
    if (tabs_showing) {
        buffers_get_tab_info(&vm->tabs.labels, &vm->tabs.count, &vm->tabs.active);
    }

    /* Row views */
    int available_rows = ctx->view.screenrows - tabs_showing;
    vm->row_count = available_rows;
    vm->row_views = calloc(available_rows, sizeof(EditorRowView));
    if (!vm->row_views) {
        editor_viewmodel_free(vm);
        return NULL;
    }

    for (int y = 0; y < available_rows; y++) {
        if (copy_row_view(&vm->row_views[y], ctx, y, vm->gutter_width, text_cols) < 0) {
            editor_viewmodel_free(vm);
            return NULL;
        }
    }

    /* Status bar */
    const char *mode_str = "";
    int link_active = loki_link_is_initialized(ctx) && loki_link_is_enabled(ctx);
    switch (ctx->view.mode) {
        case MODE_NORMAL: mode_str = link_active ? "LINK" : "NORMAL"; break;
        case MODE_INSERT: mode_str = "INSERT"; break;
        case MODE_VISUAL: mode_str = "VISUAL"; break;
        case MODE_COMMAND: mode_str = "COMMAND"; break;
        case MODE_PICKER: mode_str = "PICKER"; break;
    }

    /* Get language indicator */
    const LokiLangOps *lang = loki_lang_for_file(ctx->model.filename);
    char lang_buf[16] = "";
    if (lang && lang->is_initialized && lang->is_initialized(ctx)) {
        snprintf(lang_buf, sizeof(lang_buf), "%s ", lang->name);
        for (int i = 0; lang_buf[i] && i < 15; i++) {
            if (lang_buf[i] >= 'a' && lang_buf[i] <= 'z') {
                lang_buf[i] -= 32;
            }
        }
    }

    /* Copy status info with owned strings */
    vm->status_mode = safe_strdup(mode_str);
    vm->status_filename = safe_strdup(ctx->model.filename);
    vm->status_lang = safe_strdup(lang_buf);

    vm->status.mode = vm->status_mode;
    vm->status.filename = vm->status_filename;
    vm->status.lang = vm->status_lang;
    vm->status.numrows = ctx->model.numrows;
    vm->status.current_row = ctx->view.rowoff + ctx->view.cy + 1;
    vm->status.dirty = ctx->model.dirty;
    vm->status.playing = loki_lang_is_playing(ctx);
    vm->status.link_active = link_active;

    /* Message line */
    if (ctx->view.statusmsg[0] && time(NULL) - ctx->view.statusmsg_time < 5) {
        vm->message = safe_strdup(ctx->view.statusmsg);
    }

    /* REPL pane */
    t_lua_repl *repl = ctx_repl(ctx);
    if (repl && repl->active) {
        vm->repl_active = 1;
        vm->repl_prompt = safe_strdup(LUA_REPL_PROMPT);
        vm->repl_input = repl->input_len > 0 ? strndup(repl->input, repl->input_len) : NULL;

        /* Copy log lines */
        vm->repl_log_count = repl->log_len;
        if (repl->log_len > 0) {
            vm->repl_log = calloc(repl->log_len, sizeof(char *));
            if (vm->repl_log) {
                for (int i = 0; i < repl->log_len; i++) {
                    vm->repl_log[i] = safe_strdup(repl->log[i]);
                }
            }
        }

        vm->repl.prompt = vm->repl_prompt;
        vm->repl.input = vm->repl_input;
        vm->repl.input_len = repl->input_len;
        vm->repl.log_lines = (const char **)vm->repl_log;
        vm->repl.log_count = vm->repl_log_count;
        vm->repl.max_display_lines = LUA_REPL_OUTPUT_ROWS;
    }

    /* Cursor position */
    if (repl && repl->active) {
        int prompt_len = (int)strlen(LUA_REPL_PROMPT);
        int visible = repl->input_len;
        if (prompt_len + visible >= ctx->view.screencols) {
            visible = ctx->view.screencols > prompt_len ? ctx->view.screencols - prompt_len : 0;
        }
        vm->cursor.row = ctx->view.screenrows + STATUS_ROWS + LUA_REPL_OUTPUT_ROWS + 1;
        vm->cursor.col = prompt_len + visible + 1;
        if (vm->cursor.col < 1) vm->cursor.col = 1;
        if (vm->cursor.col > ctx->view.screencols) vm->cursor.col = ctx->view.screencols;
        vm->cursor.visible = 1;
    } else {
        int cx = 1;
        int filerow = ctx->view.rowoff + ctx->view.cy;
        t_erow *row = (filerow < 0 || filerow >= ctx->model.numrows) ? NULL : &ctx->model.row[filerow];
        if (row) {
            for (int j = ctx->view.coloff; j < (ctx->view.cx + ctx->view.coloff); j++) {
                if (j < row->size && row->chars[j] == TAB)
                    cx += 7 - ((cx) % 8);
                cx++;
            }
        }
        if (ctx->view.line_numbers && ctx->model.numrows > 0) {
            cx += vm->gutter_width;
        }
        int tab_offset = tabs_showing ? 1 : 0;
        vm->cursor.row = ctx->view.cy + 1 + tab_offset;
        vm->cursor.col = cx;
        if (vm->cursor.col > ctx->view.screencols) vm->cursor.col = ctx->view.screencols;
        vm->cursor.file_row = filerow;
        vm->cursor.file_col = ctx->view.coloff + ctx->view.cx;
        vm->cursor.visible = 1;
    }

    return vm;
}

void editor_viewmodel_free(EditorViewModel *vm) {
    if (!vm) return;

    /* Free row views */
    if (vm->row_views) {
        for (int i = 0; i < vm->row_count; i++) {
            free_row_view(&vm->row_views[i]);
        }
        free(vm->row_views);
    }

    /* Free tab info */
    buffers_free_tab_info(vm->tabs.labels, vm->tabs.count);

    /* Free status strings */
    free(vm->status_mode);
    free(vm->status_filename);
    free(vm->status_lang);

    /* Free message */
    free(vm->message);

    /* Free REPL data */
    free(vm->repl_prompt);
    free(vm->repl_input);
    if (vm->repl_log) {
        for (int i = 0; i < vm->repl_log_count; i++) {
            free(vm->repl_log[i]);
        }
        free(vm->repl_log);
    }

    free(vm);
}

/* ======================= Convenience Accessors ============================= */

EditorMode editor_session_get_mode(EditorSession *session) {
    if (!session) return MODE_NORMAL;
    return session->ctx.view.mode;
}

int editor_session_is_dirty(EditorSession *session) {
    if (!session) return 0;
    return session->ctx.model.dirty != 0;
}

const char *editor_session_get_filename(EditorSession *session) {
    if (!session) return NULL;
    return session->ctx.model.filename;
}

void editor_session_resize(EditorSession *session, int rows, int cols) {
    if (!session) return;
    session->ctx.view.screenrows = rows;
    session->ctx.view.screencols = cols;
}

int editor_session_open(EditorSession *session, const char *filename) {
    if (!session || !filename) return -1;
    return editor_open(&session->ctx, (char *)filename);
}

int editor_session_save(EditorSession *session) {
    if (!session) return -1;
    return editor_save(&session->ctx);
}

editor_ctx_t *editor_session_get_ctx(EditorSession *session) {
    if (!session) return NULL;
    return &session->ctx;
}
