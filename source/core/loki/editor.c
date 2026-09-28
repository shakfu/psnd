/* loki_editor.c - Integration layer between editor core and Lua
 *
 * This file contains:
 * - Lua state management
 * - REPL state and functions
 * - Main editor loop with Lua integration
 * - Functions that bridge between pure C core and Lua bindings
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
#include <io.h>
#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#else
#include <unistd.h>
#endif
#include <time.h>

/* Lua headers */
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

/* Loki headers */
#include "psnd.h"
#include "loki/editor.h"
#include "loki/core.h"
#include "loki/lua.h"
#include "internal.h"
#include "terminal.h"
#include "buffers.h"
#include "syntax.h"
#include "config.h"
#include "theme_toml.h"
#include "lang_toml.h"
#include "keybind.h"
#ifdef LOKI_USE_LINENOISE
#include "treesitter.h"
#endif
#include "lang_bridge.h"
#include "loki/link.h"
#include "live_loop.h"
#include "async_queue.h"
#include "command/command_impl.h"
#include "shared/context.h"
#include "shared/osc/osc.h"
#ifdef SHARED_SOURCE_TRACKING
#include "async/shared_async.h"
#endif
#ifdef BUILD_MINIHOST_BACKEND
#include "shared/audio/minihost_backend.h"
#endif

/* ======================== Main Editor Instance ============================ */

/* Note: Editor context is now local to loki_editor_main() and managed by
 * the buffer manager after initialization. No static instance needed. */

/* ======================== Helper Functions =============================== */

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

/* Lua status reporter - reports Lua errors to editor status bar */
static void loki_lua_status_reporter(const char *message, void *userdata) {
    editor_ctx_t *ctx = (editor_ctx_t *)userdata;
    if (message && message[0] != '\0' && ctx) {
        editor_set_status_msg(ctx, "%s", message);
    }
}

/* Update REPL layout when active/inactive state changes */
void editor_update_repl_layout(editor_ctx_t *ctx) {
    if (!ctx) return;
    int reserved = (ctx_repl(ctx) && ctx_repl(ctx)->active) ? LUA_REPL_TOTAL_ROWS : 0;
    int available = ctx->view.screenrows_total;
    if (available > reserved) {
        ctx->view.screenrows = available - reserved;
    } else {
        ctx->view.screenrows = 1;
    }
    if (ctx->view.screenrows < 1) ctx->view.screenrows = 1;

    if (ctx->view.cy >= ctx->view.screenrows) {
        ctx->view.cy = ctx->view.screenrows - 1;
        if (ctx->view.cy < 0) ctx->view.cy = 0;
    }

    if (ctx->model.numrows > ctx->view.screenrows && ctx->view.rowoff > ctx->model.numrows - ctx->view.screenrows) {
        ctx->view.rowoff = ctx->model.numrows - ctx->view.screenrows;
    }
    if (ctx->model.numrows <= ctx->view.screenrows) {
        ctx->view.rowoff = 0;
    }
}

/* Toggle the Lua REPL focus */
static void exec_lua_command(editor_ctx_t *ctx, int fd) {
    (void)fd;
    if (!ctx || !ctx_L(ctx) || !ctx_repl(ctx)) {
        editor_set_status_msg(ctx, "Lua not available");
        return;
    }
    t_lua_repl *repl = ctx_repl(ctx);
    int was_active = repl->active;
    repl->active = !repl->active;
    editor_update_repl_layout(ctx);
    if (repl->active) {
        repl->history_index = -1;
        editor_set_status_msg(ctx,
            "Lua REPL: Enter runs, ESC exits, Up/Down history, type 'help'");
        if (repl->log_len == 0) {
            lua_repl_append_log(ctx, "Type 'help' for built-in commands");
        }
    } else {
        if (was_active) {
            editor_set_status_msg(ctx, "Lua REPL closed");
        }
    }
}

/* Apply Lua-based highlighting spans to a row */
static int lua_apply_span_table(editor_ctx_t *ctx, t_erow *row, int table_index) {
    if (!ctx || !ctx_L(ctx)) return 0;
    lua_State *L = ctx_L(ctx);
    if (!lua_istable(L, table_index)) return 0;

    int applied = 0;
    size_t entries = lua_rawlen(L, table_index);

    for (size_t i = 1; i <= entries; i++) {
        lua_rawgeti(L, table_index, (lua_Integer)i);
        if (lua_type(L, -1) == LUA_TTABLE) {
            int start = 0;
            int stop = 0;
            int length = 0;
            int style = -1;

            lua_getfield(L, -1, "start");
            if (lua_isnumber(L, -1)) start = (int)lua_tointeger(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "stop");
            if (lua_isnumber(L, -1)) stop = (int)lua_tointeger(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "end");
            if (lua_isnumber(L, -1)) stop = (int)lua_tointeger(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "length");
            if (lua_isnumber(L, -1)) length = (int)lua_tointeger(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, -1, "style");
            if (lua_isstring(L, -1)) {
                style = syntax_name_to_code(lua_tostring(L, -1));
            } else if (lua_isnumber(L, -1)) {
                style = (int)lua_tointeger(L, -1);
            }
            lua_pop(L, 1);

            if (style < 0) {
                lua_getfield(L, -1, "type");
                if (lua_isstring(L, -1)) {
                    style = syntax_name_to_code(lua_tostring(L, -1));
                } else if (lua_isnumber(L, -1)) {
                    style = (int)lua_tointeger(L, -1);
                }
                lua_pop(L, 1);
            }

            if (start <= 0) start = 1;
            if (length > 0 && stop <= 0) stop = start + length - 1;
            if (stop <= 0) stop = start;

            if (style >= 0 && row->rsize > 0) {
                if (start > stop) {
                    int tmp = start;
                    start = stop;
                    stop = tmp;
                }
                if (start < 1) start = 1;
                if (stop > row->rsize) stop = row->rsize;
                for (int pos = start - 1; pos < stop && pos < row->rsize; pos++) {
                    row->hl[pos] = style;
                }
                applied = 1;
            } else if (style >= 0 && row->rsize == 0) {
                applied = 1;
            }
        }
        lua_pop(L, 1);
    }

    return applied;
}

/* Apply Lua custom highlighting to a row */
static void lua_apply_highlight_row(editor_ctx_t *ctx, t_erow *row, int default_ran) {
    if (!ctx || !ctx_L(ctx) || row == NULL || row->render == NULL) return;
    lua_State *L = ctx_L(ctx);
    int top = lua_gettop(L);

    lua_getglobal(L, "loki");
    if (!lua_istable(L, -1)) {
        lua_settop(L, top);
        return;
    }

    lua_getfield(L, -1, "highlight_row");
    if (!lua_isfunction(L, -1)) {
        lua_settop(L, top);
        return;
    }

    lua_pushinteger(L, row->idx);
    lua_pushlstring(L, row->chars ? row->chars : "", (size_t)row->size);
    lua_pushlstring(L, row->render ? row->render : "", (size_t)row->rsize);
    if (ctx->view.syntax) {
        lua_pushinteger(L, ctx->view.syntax->type);
    } else {
        lua_pushnil(L);
    }
    lua_pushboolean(L, default_ran);

    if (lua_pcall(L, 5, 1, 0) != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        editor_set_status_msg(ctx, "Lua highlight error: %s", err ? err : "unknown");
        lua_settop(L, top);
        return;
    }

    if (!lua_istable(L, -1)) {
        lua_settop(L, top);
        return;
    }

    int table_index = lua_gettop(L);
    int replace = 0;

    lua_getfield(L, table_index, "replace");
    if (lua_isboolean(L, -1)) replace = lua_toboolean(L, -1);
    lua_pop(L, 1);

    int spans_index = table_index;
    int has_spans_field = 0;

    lua_getfield(L, table_index, "spans");
    if (lua_istable(L, -1)) {
        spans_index = lua_gettop(L);
        has_spans_field = 1;
    } else {
        lua_pop(L, 1);
    }

    if (replace) {
        memset(row->hl, HL_NORMAL, row->rsize);
    }

    lua_apply_span_table(ctx, row, spans_index);

    if (has_spans_field) {
        lua_pop(L, 1);
    }

    lua_settop(L, top);
}

/* ======================== Main Editor Function =========================== */

static void print_usage(void) {
    printf("Usage: " PSND_NAME " [options] <filename>\n");
    printf("\nOptions:\n");
    printf("  -h, --help          Show this help message\n");
    printf("  -v, --version       Show version information\n");
    printf("  -sf PATH            Use built-in synth with soundfont (.sf2)\n");
    printf("  -cs PATH            Use Csound synthesis with .csd file\n");
#ifdef BUILD_MINIHOST_BACKEND
    printf("  -pg, --plugin PATH  Use VST3/AU plugin for synthesis\n");
    printf("  --plugin-log PATH   Write plugin debug output to file\n");
#endif
#ifdef PSND_OSC
    printf("\nOSC (Open Sound Control):\n");
    printf("  --osc               Enable OSC server (default port: %d)\n", PSND_OSC_DEFAULT_PORT);
    printf("  --osc-port N        OSC server port\n");
    printf("  --osc-send H:P      Broadcast events to host:port\n");
#endif
    printf("\nInteractive mode (default):\n");
    printf("  " PSND_NAME " <file.alda>           Open file in editor\n");
    printf("  " PSND_NAME " -sf gm.sf2 song.alda  Open with TinySoundFont synth\n");
    printf("  " PSND_NAME " -cs inst.csd song.alda Open with Csound synthesis\n");
    printf("\nKeybindings:\n");
    printf("  Ctrl-E    Play current part or selection\n");
    printf("  Ctrl-P    Play entire file\n");
    printf("  Ctrl-G    Stop playback\n");
    printf("  Ctrl-S    Save file\n");
    printf("  Ctrl-Q    Quit\n");
    printf("  Ctrl-F    Find\n");
    printf("  Ctrl-L    Lua console\n");
}

/* Load a VST3/AU plugin into slot 0 and route playback through it. Reports
 * the outcome in the status bar; returns 0 on success. Shared by the terminal
 * editor and EditorSession (--web, --native). */
int editor_load_plugin(editor_ctx_t *ctx, const char *path, const char *log_path) {
#ifdef BUILD_MINIHOST_BACKEND
    if (!ctx->model.shared) return -1;
    /* Set log file before loading (NULL = suppress to /dev/null) */
    shared_minihost_set_log_file(log_path);
    if (shared_minihost_init() != 0) {
        editor_set_status_msg(ctx, "Failed to initialize plugin host");
        return -1;
    }
    if (shared_minihost_load(0, path) != 0) {
        editor_set_status_msg(ctx, "Failed to load plugin: %s", path);
        return -1;
    }
    if (shared_minihost_enable() != 0) {
        editor_set_status_msg(ctx, "Failed to enable plugin audio");
        return -1;
    }
    ctx->model.shared->minihost_enabled = 1;
    const char *name = shared_minihost_get_plugin_name(0);
    editor_set_status_msg(ctx, "Plugin loaded: %s", name ? name : path);
    return 0;
#else
    (void)log_path;
    editor_set_status_msg(ctx, "Plugin support not built: %s", path);
    return -1;
#endif
}

int loki_editor_main(int argc, char **argv) {
    /* Static editor context - ensures all fields are zero-initialized.
     * This is critical because init_editor() doesn't initialize all fields
     * (undo_state, indent_config, language states) and Lua bootstrap
     * may access these fields before buffers_init() runs. */
    static editor_ctx_t E;

    /* Initialize language bridge system */
    loki_lang_init();

    /* Initialize async event queue */
    if (async_queue_init() != 0) {
        fprintf(stderr, "Warning: Failed to initialize async event queue\n");
    }

    /* Register cleanup handler early to ensure terminal is always restored */
    atexit(editor_atexit);

    /* Parse command-line arguments */
    const char *filename = NULL;
    const char *soundfont_path = NULL;
    const char *csound_path = NULL;
    const char *plugin_path = NULL;
    const char *plugin_log = NULL;
    int osc_enabled = 0;
    int osc_port = 0;
    const char *osc_send_host = NULL;
    const char *osc_send_port = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage();
            exit(0);
        }
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf(PSND_NAME " %s\n", PSND_VERSION);
            exit(0);
        }
        if (strcmp(argv[i], "-sf") == 0 && i + 1 < argc) {
            soundfont_path = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "-cs") == 0 && i + 1 < argc) {
            csound_path = argv[++i];
            continue;
        }
        if ((strcmp(argv[i], "--plugin") == 0 || strcmp(argv[i], "-pg") == 0) && i + 1 < argc) {
            plugin_path = argv[++i];
            continue;
        }
        if (strcmp(argv[i], "--plugin-log") == 0 && i + 1 < argc) {
            plugin_log = argv[++i];
            continue;
        }
        /* OSC options */
        if (strcmp(argv[i], "--osc") == 0) {
            osc_enabled = 1;
            continue;
        }
        if (strcmp(argv[i], "--osc-port") == 0 && i + 1 < argc) {
            osc_port = atoi(argv[++i]);
            osc_enabled = 1;
            continue;
        }
        if (strcmp(argv[i], "--osc-send") == 0 && i + 1 < argc) {
            const char *target = argv[++i];
            const char *colon = strrchr(target, ':');
            if (!colon || colon == target) {
                fprintf(stderr, "Error: --osc-send requires host:port format\n");
                exit(1);
            }
            static char osc_host_buf[256];
            size_t host_len = colon - target;
            if (host_len >= sizeof(osc_host_buf)) {
                host_len = sizeof(osc_host_buf) - 1;
            }
            strncpy(osc_host_buf, target, host_len);
            osc_host_buf[host_len] = '\0';
            osc_send_host = osc_host_buf;
            osc_send_port = colon + 1;
            osc_enabled = 1;
            continue;
        }
        if (argv[i][0] == '-') {
            fprintf(stderr, "Error: Unknown option: %s\n", argv[i]);
            print_usage();
            exit(1);
        }
        /* Non-option argument is the filename */
        if (filename == NULL) {
            filename = argv[i];
        } else {
            fprintf(stderr, "Error: Too many arguments\n");
            print_usage();
            exit(1);
        }
    }

    /* Suppress unused variable warnings when OSC is not compiled in */
    (void)osc_enabled;
    (void)osc_port;
    (void)osc_send_host;
    (void)osc_send_port;

    if (filename == NULL) {
        print_usage();
        exit(1);
    }

    /* Initialize editor core */
    init_editor(&E);
    syntax_select_for_filename(&E, filename);
    editor_open(&E, filename);

    /* Load TOML configuration first (before Lua) */
    loki_config_t *config = config_global();

    /* Apply config settings to editor */
    E.view.line_numbers = config->line_numbers ? 1 : 0;

    /* Load theme from config (TOML themes only - Lua themes require Lua) */
    if (config->theme[0] != '\0') {
        theme_toml_load(&E, config->theme);
    }

    /* Load TOML language definitions (for syntax highlighting without Lua) */
    lang_toml_load_all();

    /* Initialize configurable keybindings */
    keybind_init();

    /* Initialize LuaHost only if Lua is enabled in config */
    LuaHost *lua_host = NULL;
    if (config->lua_enabled) {
        lua_host = lua_host_create();
        if (!lua_host) {
            fprintf(stderr, "Warning: Failed to allocate LuaHost\n");
        } else {
            struct loki_lua_opts opts = {
                .bind_editor = 1,
                .bind_http = 0,
                .load_config = 1,
                .config_override = NULL,
                .project_root = NULL,
                .extra_lua_path = NULL,
                .reporter = loki_lua_status_reporter,
                .reporter_userdata = NULL
            };

            /* Set lua_host on E before bootstrap (so bootstrap can find context) */
            E.lua_host = lua_host;

            lua_host->L = loki_lua_bootstrap(&E, &opts);
            if (!lua_host->L) {
                fprintf(stderr, "Warning: Failed to initialize Lua runtime (%s)\n", loki_lua_runtime());
            }

            /* Initialize REPL */
            lua_host_init_repl(lua_host);
        }
    }

    /* Re-select syntax now that Lua has registered dynamic languages */
    if (!E.view.syntax && E.model.filename) {
        syntax_select_for_filename(&E, E.model.filename);
        /* If syntax was found, refresh highlighting for all rows */
        if (E.view.syntax) {
            for (int i = 0; i < E.model.numrows; i++) {
                syntax_update_row(&E, &E.model.row[i]);
            }
        }
    }

    /* Initialize buffer management with the initial editor context */
    if (buffers_init(&E) != 0) {
        fprintf(stderr, "Error: Failed to initialize buffer management\n");
        exit(1);
    }

    /* Update atexit context to point to buffer manager's context (not local E) */
    editor_set_atexit_context(buffer_get_current());

    /* Auto-initialize language for known file types (must be after buffers_init) */
    {
        editor_ctx_t *ctx = buffer_get_current();
        if (ctx) {
            /* Create editor-owned SharedContext for all languages to share.
             * This centralizes audio/MIDI/Link state so switching between
             * language buffers doesn't cause conflicts. */
            if (!ctx->model.shared) {
                ctx->model.shared = (SharedContext *)malloc(sizeof(SharedContext));
                if (ctx->model.shared) {
                    if (shared_context_init(ctx->model.shared) != 0) {
                        fprintf(stderr, "Warning: Failed to initialize shared context\n");
                        free(ctx->model.shared);
                        ctx->model.shared = NULL;
                    }
                } else {
                    fprintf(stderr, "Warning: Failed to allocate shared context\n");
                }
            }

            /* Initialize OSC if requested */
#ifdef PSND_OSC
            if (osc_enabled && ctx->model.shared) {
                int effective_port = osc_port > 0 ? osc_port : PSND_OSC_DEFAULT_PORT;
                if (shared_osc_init(ctx->model.shared, effective_port) == 0) {
                    if (osc_send_host && osc_send_port) {
                        shared_osc_set_broadcast(ctx->model.shared, osc_send_host, osc_send_port);
                    }
                    /* Set up user data and lang callbacks for OSC handlers */
                    shared_osc_set_user_data(ctx->model.shared, ctx);
                    shared_osc_set_lang_callbacks(loki_lang_eval, loki_lang_eval_buffer, loki_lang_stop_all);
                    /* Set up query callbacks for OSC query/reply handlers */
                    shared_osc_set_query_callbacks(loki_lang_is_playing, osc_query_get_filename, osc_query_get_position);
                    if (shared_osc_start(ctx->model.shared) == 0) {
                        editor_set_status_msg(ctx, "OSC listening on port %d", effective_port);
                    } else {
                        fprintf(stderr, "Warning: Failed to start OSC server\n");
                    }
                } else {
                    fprintf(stderr, "Warning: Failed to initialize OSC on port %d\n", effective_port);
                }
            }
#endif

            /* Load VST3/AU plugin if requested via CLI */
            if (plugin_path) {
                editor_load_plugin(ctx, plugin_path, plugin_log);
            }

            int ret = loki_lang_init_for_file(ctx);
            if (ret == 0) {
                const LokiLangOps *lang = loki_lang_for_file(ctx->model.filename);
                if (lang) {
                    /* Configure audio backend if requested via CLI */
                    int backend_ret = loki_lang_configure_backend(ctx, soundfont_path, csound_path);
                    if (backend_ret == 0) {
                        /* Backend configured successfully */
                        if (csound_path) {
                            editor_set_status_msg(ctx, "%s: Using Csound (%s)", lang->name, csound_path);
                        } else if (soundfont_path) {
                            editor_set_status_msg(ctx, "%s: Using TinySoundFont (%s)", lang->name, soundfont_path);
                        }
                    } else if (backend_ret == -1) {
                        /* Backend requested but failed */
                        const char *err = loki_lang_get_error(ctx);
                        if (csound_path) {
                            editor_set_status_msg(ctx, "Failed to load CSD: %s", err ? err : csound_path);
                        } else if (soundfont_path) {
                            editor_set_status_msg(ctx, "Failed to load soundfont: %s", err ? err : soundfont_path);
                        }
                    } else if (!plugin_path) {
                        /* No backend requested - show default message */
                        editor_set_status_msg(ctx, "%s: Ctrl-E eval, Ctrl-G stop", lang->name);
                    }
                    /* If plugin_path was set, status was already shown above */
                }
            } else if (ret == -1) {
                const char *err = loki_lang_get_error(ctx);
                editor_set_status_msg(ctx, "Language init failed: %s", err ? err : "unknown error");
            }
            /* ret == 1 means no language for this file type, which is fine */
        }
    }

    /* Initialize terminal host and enable raw mode */
    terminal_host_init(g_terminal_host, STDIN_FILENO);
    terminal_host_enable_raw_mode(g_terminal_host);
    editor_set_status_msg(buffer_get_current(),
        "HELP: Ctrl-S = save | Ctrl-Q = quit | Ctrl-F = find | Ctrl-T = new buf | Ctrl-X n/p/k = buf nav");

    while(1) {
        /* Get current buffer context */
        editor_ctx_t *ctx = buffer_get_current();
        if (!ctx) {
            fprintf(stderr, "Error: No active buffer\n");
            exit(1);
        }

        terminal_handle_resize(ctx);

        /* Check live loops for beat boundary triggers (pushes events to queue) */
        live_loop_tick();

        /* Tick metronome if enabled (plays clicks on beat boundaries) */
        if (metronome_is_enabled()) {
            metronome_tick(ctx);
        }

        /* Dispatch all pending async events.
         * This handles:
         * - Link callbacks (tempo, peers, transport changes)
         * - Beat boundary events (live loop triggers)
         * - Language playback completion callbacks
         * - Custom events */
        if (ctx_L(ctx)) {
            async_queue_dispatch_lua(NULL, ctx, ctx_L(ctx));

            /* Update language slot state (mark completed slots).
             * Lua callback invocation is handled by async_queue_dispatch_lua. */
            loki_lang_check_callbacks(ctx, ctx_L(ctx));
        }

#ifdef SHARED_SOURCE_TRACKING
        /* Update playing line for playback visualization.
         * First try language-specific source line (e.g., Bog scheduler),
         * then fall back to shared async infrastructure (Alda, Joy). */
        int lang_source_line = loki_lang_get_source_line(ctx);
        if (lang_source_line > 0) {
            ctx->view.playing_line = lang_source_line;
        } else {
            ctx->view.playing_line = shared_async_get_current_source_line(-1);
        }
#endif

        editor_refresh_screen(ctx);
        editor_process_keypress(ctx, STDIN_FILENO);
    }

    return 0;
}

/* Clean up editor resources (called from editor_atexit in loki_core.c) */
void editor_cleanup_resources(editor_ctx_t *ctx) {
    if (!ctx) return;

    /* Stop all live loops */
    live_loop_shutdown();

    /* Clean up all language subsystems (stops all playback) */
    loki_lang_cleanup_all(ctx);

    /* Clean up editor-owned SharedContext after languages are done */
    if (ctx->model.shared) {
        shared_context_cleanup(ctx->model.shared);
        free(ctx->model.shared);
        ctx->model.shared = NULL;
    }

#ifdef LOKI_USE_LINENOISE
    /* Clean up tree-sitter state */
    if (ctx->model.ts_state) {
        treesitter_free(ctx->model.ts_state);
        ctx->model.ts_state = NULL;
    }
#endif

    /* Clean up async event queue */
    async_queue_cleanup();

    /* Clean up LuaHost (includes REPL and Lua state) */
    if (ctx->lua_host) {
        lua_host_free(ctx->lua_host);
        ctx->lua_host = NULL;
    }
}
