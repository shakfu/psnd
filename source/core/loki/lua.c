/* loki_lua.c - Lua API bindings for Loki editor
 *
 * This file contains all Lua C bindings that expose editor functionality
 * to Lua scripts. These bindings allow users to extend and customize the
 * editor through Lua configuration files and REPL commands.
 *
 * Dependencies:
 * - Standard C headers for I/O, strings, memory management
 * - Lua 5.4 headers for the C API
 * - Loki headers for core editor functionality and types
 *
 * Note: This code references global editor state (struct E) and various
 * functions from loki_core.c. These dependencies need to be properly
 * exposed through loki/core.h or kept in loki_core.c.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define access _access
#define F_OK 0
#define R_OK 4
#else
#include <strings.h>  /* for strcasecmp */
#include <unistd.h>   /* for access */
#endif
#include <stdarg.h>
#include <ctype.h>    /* for isspace, isprint, tolower */

#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

#include "psnd.h"
#include "loki/core.h"
#include "loki/lua.h"
#include "internal.h"  /* Internal structures and functions */
#include "terminal.h"  /* Terminal functions */
#include "languages.h"  /* Language definitions and dynamic registration */
#include "command.h"  /* Command mode and ex-style commands */
#include "lang_bridge.h"  /* Language bridge for Lua API registration */
#include "loki/link.h"       /* Ableton Link integration */
#include "export.h"          /* MIDI export control */
#include "buffers.h"    /* Buffer management for buffer_get_current() */
#include "syntax.h"     /* syntax_name_to_code() for theme color mapping */
#include "shared/context.h"  /* SharedContext for launch_quantize */
#include "shared/midi/midi.h"  /* MIDI port functions */
#include "shared/osc/osc.h"   /* OSC functions */
#include "shared/param/param.h"  /* Parameter system */
#include "shared/audio/audio.h"  /* Audio backends including minihost */

#ifdef BUILD_PLUGIN_SQLITE
#include "lua_fts.h"  /* SQLite FTS5 search index plugin */
#endif

/* ======================= Lua API bindings ================================ */

/* Registry key for storing editor context pointer.
 * This is used to retrieve the context in Lua C API functions.
 * The key itself is just the address of this variable - we don't care about
 * its value, just that it's a unique pointer for the registry. */
static const char editor_ctx_registry_key = 0;

/* Helper function to get current editor context.
 * Tries buffer_get_current() first for multi-buffer support.
 * Falls back to registry pointer for tests/single-buffer mode.
 * Exported for language Lua bindings (alda.c, joy.c). */
editor_ctx_t* loki_lua_get_editor_context(lua_State *L) {
    /* Try buffer system first (for normal editor operation) */
    editor_ctx_t *ctx = buffer_get_current();
    if (ctx) return ctx;

    /* Fallback to registry pointer (for tests or single-buffer mode) */
    lua_pushlightuserdata(L, (void *)&editor_ctx_registry_key);
    lua_gettable(L, LUA_REGISTRYINDEX);
    ctx = (editor_ctx_t *)lua_touserdata(L, -1);
    lua_pop(L, 1);
    return ctx;
}

/* Lua API: loki.status(message) - Set status message */
static int lua_loki_status(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) return 0;

    const char *msg = luaL_checkstring(L, 1);
    editor_set_status_msg(ctx, "%s", msg);
    return 0;
}

/* Lua API: loki.get_line(row) - Get line content at row (0-indexed) */
static int lua_loki_get_line(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) return 0;

    int row = luaL_checkinteger(L, 1);
    if (row < 0 || row >= ctx->model.numrows) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushstring(L, ctx->model.row[row].chars);
    return 1;
}

/* Lua API: loki.get_lines() - Get total number of lines */
static int lua_loki_get_lines(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) return 0;

    lua_pushinteger(L, ctx->model.numrows);
    return 1;
}

/* Lua API: loki.get_cursor() - Get cursor position (returns row, col)
 * Returns FILE position, not screen position (accounts for scroll offset) */
static int lua_loki_get_cursor(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) return 0;

    /* Return file position: screen pos + scroll offset */
    lua_pushinteger(L, ctx->view.rowoff + ctx->view.cy);
    lua_pushinteger(L, ctx->view.coloff + ctx->view.cx);
    return 2;
}

/* Lua API: loki.insert_text(text) - Insert text at cursor */
static int lua_loki_insert_text(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) return 0;

    const char *text = luaL_checkstring(L, 1);
    for (const char *p = text; *p; p++) {
        editor_insert_char(ctx, *p);
    }
    return 0;
}

/* Lua API: loki.stream_text(text) - Append text and scroll to bottom */
static int lua_loki_stream_text(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) return 0;

    const char *text = luaL_checkstring(L, 1);

    /* Move to end of file. cy/cx are screen-relative, so place the last
     * row on screen first; otherwise a scrolled view inserts past the end. */
    if (ctx->model.numrows > 0) {
        int last = ctx->model.numrows - 1;
        ctx->view.rowoff = last >= ctx->view.screenrows && ctx->view.screenrows > 0
                         ? last - ctx->view.screenrows + 1 : 0;
        ctx->view.cy = last - ctx->view.rowoff;
        ctx->view.coloff = 0;
        ctx->view.cx = ctx->model.row[last].size;
    }

    /* Insert the text */
    for (const char *p = text; *p; p++) {
        editor_insert_char(ctx, *p);
    }

    /* Scroll to bottom */
    if (ctx->model.numrows > ctx->view.screenrows) {
        ctx->view.rowoff = ctx->model.numrows - ctx->view.screenrows;
    } else {
        ctx->view.rowoff = 0;
    }
    /* cy is screen-relative, and an empty buffer must not leave it negative. */
    ctx->view.cy = ctx->model.numrows - 1 - ctx->view.rowoff;
    if (ctx->view.cy < 0) ctx->view.cy = 0;

    /* Refresh screen immediately */
    editor_refresh_screen(ctx);

    return 0;
}

/* Lua API: loki.get_filename() - Get current filename */
static int lua_loki_get_filename(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) return 0;

    if (ctx->model.filename) {
        lua_pushstring(L, ctx->model.filename);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

/* Helper: Map color name to HL_* constant
 * Delegates to syntax_name_to_code() which supports all 51 HL_* types.
 * See hl_types.h for the full list of supported names. */
static int color_name_to_hl(const char *name) {
    return syntax_name_to_code(name);
}

/* Lua API: loki.set_color(name, {r=R, g=G, b=B}) - Set syntax highlight color */
static int lua_loki_set_color(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) return 0;

    const char *name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    int hl = color_name_to_hl(name);
    if (hl < 0) {
        return luaL_error(L, "Unknown color name: %s", name);
    }

    /* Get RGB values from table */
    lua_getfield(L, 2, "r");
    lua_getfield(L, 2, "g");
    lua_getfield(L, 2, "b");

    if (!lua_isnumber(L, -3) || !lua_isnumber(L, -2) || !lua_isnumber(L, -1)) {
        return luaL_error(L, "Color table must have r, g, b numeric fields");
    }

    int r = lua_tointeger(L, -3);
    int g = lua_tointeger(L, -2);
    int b = lua_tointeger(L, -1);

    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
        return luaL_error(L, "RGB values must be 0-255");
    }

    ctx->view.colors[hl].r = r;
    ctx->view.colors[hl].g = g;
    ctx->view.colors[hl].b = b;

    lua_pop(L, 3);
    return 0;
}

/* Lua API: loki.set_theme(table) - Set multiple colors at once */
static int lua_loki_set_theme(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) return 0;

    luaL_checktype(L, 1, LUA_TTABLE);

    /* Iterate over theme table */
    lua_pushnil(L);
    while (lua_next(L, 1) != 0) {
        /* Key at -2, value at -1 */
        if (lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TTABLE) {
            const char *name = lua_tostring(L, -2);
            int hl = color_name_to_hl(name);

            if (hl >= 0) {
                /* Get RGB from value table */
                lua_getfield(L, -1, "r");  /* Stack: ... table, r */
                lua_getfield(L, -2, "g");  /* Stack: ... table, r, g */
                lua_getfield(L, -3, "b");  /* Stack: ... table, r, g, b */

                if (lua_isnumber(L, -3) && lua_isnumber(L, -2) && lua_isnumber(L, -1)) {
                    int r = lua_tointeger(L, -3);
                    int g = lua_tointeger(L, -2);
                    int b = lua_tointeger(L, -1);

                    if (r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
                        ctx->view.colors[hl].r = r;
                        ctx->view.colors[hl].g = g;
                        ctx->view.colors[hl].b = b;
                    }
                }
                lua_pop(L, 3);
            }
        }
        lua_pop(L, 1); /* Remove value, keep key for next iteration */
    }

    return 0;
}

/* =========================== Display Settings Lua API ======================== */

/* Lua API: loki.line_numbers([enabled]) - Get or set line numbers display
 * With no argument: returns current state (true/false)
 * With boolean argument: sets line numbers on/off
 * Example: loki.line_numbers(true) to enable, loki.line_numbers(false) to disable */
static int lua_loki_line_numbers(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) return 0;

    /* If no arguments, return current state */
    if (lua_gettop(L) == 0) {
        lua_pushboolean(L, ctx->view.line_numbers);
        return 1;
    }

    /* Set line numbers based on argument */
    int enabled = lua_toboolean(L, 1);
    ctx->view.line_numbers = enabled ? 1 : 0;

    return 0;
}

/* =========================== Modal System Lua API =========================== */

/* Lua API: loki.get_mode() - Get current editor mode */
static int lua_loki_get_mode(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) return 0;

    const char *mode_str = "";
    switch(ctx->view.mode) {
        case MODE_NORMAL: mode_str = "normal"; break;
        case MODE_INSERT: mode_str = "insert"; break;
        case MODE_VISUAL: mode_str = "visual"; break;
        case MODE_COMMAND: mode_str = "command"; break;
        case MODE_PICKER: mode_str = "picker"; break;
    }
    lua_pushstring(L, mode_str);
    return 1;
}

/* Lua API: loki.set_mode(mode) - Set editor mode */
static int lua_loki_set_mode(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) return 0;

    const char *mode_str = luaL_checkstring(L, 1);

    EditorMode new_mode = ctx->view.mode;
    if (strcasecmp(mode_str, "normal") == 0) {
        new_mode = MODE_NORMAL;
    } else if (strcasecmp(mode_str, "insert") == 0) {
        new_mode = MODE_INSERT;
    } else if (strcasecmp(mode_str, "visual") == 0) {
        new_mode = MODE_VISUAL;
        /* Activate selection */
        ctx->view.sel_active = 1;
        ctx->view.sel_start_x = ctx->view.cx;
        ctx->view.sel_start_y = ctx->view.cy;
        ctx->view.sel_end_x = ctx->view.cx;
        ctx->view.sel_end_y = ctx->view.cy;
    } else if (strcasecmp(mode_str, "command") == 0) {
        new_mode = MODE_COMMAND;
    } else {
        return luaL_error(L, "Invalid mode: %s", mode_str);
    }

    ctx->view.mode = new_mode;
    return 0;
}

/* Lua API: loki.register_command(key, callback) - Register normal mode command */
static int lua_loki_register_command(lua_State *L) {
    /* This just stores commands in a Lua table for now */
    /* The actual dispatching is done by loki_process_normal_key in Lua */

    /* Get or create global command registry table */
    lua_getglobal(L, "_loki_commands");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1); /* Duplicate table */
        lua_setglobal(L, "_loki_commands");
    }

    /* Register command: _loki_commands[key] = callback */
    const char *key = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    lua_pushstring(L, key);
    lua_pushvalue(L, 2); /* Push the callback function */
    lua_settable(L, -3);

    lua_pop(L, 1); /* Pop the registry table */
    return 0;
}

/* C wrapper for Lua ex-command callbacks */
static int lua_ex_command_handler(editor_ctx_t *ctx, const char *args) {
    if (!ctx || !ctx_L(ctx)) return 0;
    lua_State *L = ctx_L(ctx);

    /* Get command name from somewhere - for now we'll use a registry */
    /* This is called from C, so we need to look up the Lua function */
    /* The function is stored in _loki_ex_commands table by command name */

    /* Get the command name from the current command being executed */
    /* We'll pass it through a global for simplicity */
    lua_getglobal(L, "_loki_ex_command_executing");
    if (!lua_isstring(L, -1)) {
        lua_pop(L, 1);
        return 0;
    }
    const char *cmd_name = lua_tostring(L, -1);
    lua_pop(L, 1);

    /* Get the callback function */
    lua_getglobal(L, "_loki_ex_commands");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return 0;
    }

    lua_getfield(L, -1, cmd_name);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 2);
        return 0;
    }

    /* Call the Lua function with args */
    lua_pushstring(L, args ? args : "");
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        const char *error = lua_tostring(L, -1);
        editor_set_status_msg(ctx, "Error: %s", error);
        lua_pop(L, 2);  /* Pop error and table */
        return 0;
    }

    /* Get return value. A handler that returns nothing (nil) has succeeded;
     * only an explicit false means failure. */
    int result = lua_isnil(L, -1) ? 1 : lua_toboolean(L, -1);
    lua_pop(L, 2);  /* Pop result and table */
    return result;
}

/* Lua API: loki.register_ex_command(name, callback, help) - Register ex-mode command
 *
 * name: Command name (without ':')
 * callback: Function that takes (args) and returns boolean (success)
 * help: Help text for the command
 *
 * Example:
 *   loki.register_ex_command("timestamp", function(args)
 *       loki.insert_text(os.date("%Y-%m-%d %H:%M:%S"))
 *       return true
 *   end, "Insert current timestamp")
 *
 * Then use: :timestamp
 */
static int lua_loki_register_ex_command(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TFUNCTION);  /* callback */
    const char *help = luaL_optstring(L, 3, "Custom command");

    /* Store callback in global registry */
    lua_getglobal(L, "_loki_ex_commands");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "_loki_ex_commands");
    }

    /* Store: _loki_ex_commands[name] = callback */
    lua_pushstring(L, name);
    lua_pushvalue(L, 2);  /* Push callback function */
    lua_settable(L, -3);
    lua_pop(L, 1);  /* Pop table */

    /* Register with C command system */
    /* We need to create a wrapper that will call the Lua function */
    /* The wrapper needs to know the command name to look up the function */

    /* For now, we'll use a simple approach: store the command name in a global
     * before calling the handler, then the handler looks it up */

    /* Register command with C system */
    int success = command_register(name, lua_ex_command_handler, help, 0, -1);

    if (!success) {
        lua_pushnil(L);
        lua_pushstring(L, "Failed to register command (already exists or registry full)");
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

static int lua_loki_repl_register(lua_State *L) {
    const char *name = luaL_checkstring(L, 1);
    const char *description = luaL_checkstring(L, 2);
    const char *example = luaL_optstring(L, 3, NULL);

    lua_getglobal(L, "loki");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return 0;
    }

    lua_getfield(L, -1, "__repl_help");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 2);
        return 0;
    }

    lua_newtable(L);
    lua_pushstring(L, name);
    lua_setfield(L, -2, "name");
    lua_pushstring(L, description);
    lua_setfield(L, -2, "description");
    if (example) {
        lua_pushstring(L, example);
        lua_setfield(L, -2, "example");
    }

    int idx = (int)lua_rawlen(L, -2) + 1;
    lua_rawseti(L, -2, idx);

    lua_pop(L, 2);
    return 0;
}

/* ============================================================================
 * Lua API Subtable Helpers
 *
 * Helper functions for registering language-specific Lua APIs under loki.*.
 * Reduces boilerplate when adding new language bindings.
 * ============================================================================ */

int loki_lua_begin_api(lua_State *L, const char *name) {
    (void)name;  /* Used by caller context, not within this function */
    lua_getglobal(L, "loki");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return 0;  /* loki table doesn't exist */
    }
    lua_newtable(L);  /* Create subtable for this language */
    return 1;
}

void loki_lua_end_api(lua_State *L, const char *name) {
    lua_setfield(L, -2, name);  /* Set subtable as loki.<name> */
    lua_pop(L, 1);              /* Pop loki table */
}

void loki_lua_add_func(lua_State *L, const char *name, lua_CFunction fn) {
    lua_pushcfunction(L, fn);
    lua_setfield(L, -2, name);
}

/* ============================================================================
 * Language Registration Helper Functions
 *
 * These functions extract and validate individual components of a language
 * definition from a Lua table. They are used by lua_loki_register_language()
 * and can be tested independently.
 * ============================================================================ */

/* Extract and validate language extensions from Lua table
 * Returns 0 on success, -1 on error (error message pushed to stack) */
static int extract_language_extensions(lua_State *L, int table_idx, struct t_editor_syntax *lang) {
    lua_getfield(L, table_idx, "extensions");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_pushstring(L, "'extensions' field is required and must be a table");
        return -1;
    }

    /* Count extensions */
    int ext_count = (int)lua_rawlen(L, -1);
    if (ext_count == 0) {
        lua_pop(L, 1);
        lua_pushstring(L, "'extensions' table cannot be empty");
        return -1;
    }

    /* Allocate extension array (NULL-terminated) */
    lang->filematch = calloc(ext_count + 1, sizeof(char*));
    if (!lang->filematch) {
        lua_pop(L, 1);
        lua_pushstring(L, "memory allocation failed for extensions");
        return -1;
    }

    /* Copy extensions */
    for (int i = 0; i < ext_count; i++) {
        lua_rawgeti(L, -1, i + 1);
        if (!lua_isstring(L, -1)) {
            lua_pop(L, 2);  /* pop string and extensions table */
            lua_pushstring(L, "extension must be a string");
            return -1;
        }
        const char *ext = lua_tostring(L, -1);
        if (ext[0] != '.') {
            lua_pop(L, 2);
            lua_pushstring(L, "extension must start with '.'");
            return -1;
        }
        lang->filematch[i] = strdup(ext);
        if (!lang->filematch[i]) {
            lua_pop(L, 2);
            lua_pushstring(L, "memory allocation failed for extension string");
            return -1;
        }
        lua_pop(L, 1);  /* pop string */
    }
    lua_pop(L, 1);  /* pop extensions table */
    return 0;
}

/* Extract and validate language keywords from Lua table
 * Returns 0 on success, -1 on error (error message pushed to stack) */
static int extract_language_keywords(lua_State *L, int table_idx, struct t_editor_syntax *lang) {
    lua_getfield(L, table_idx, "keywords");
    lua_getfield(L, table_idx, "types");

    int kw_count = lua_istable(L, -2) ? (int)lua_rawlen(L, -2) : 0;
    int type_count = lua_istable(L, -1) ? (int)lua_rawlen(L, -1) : 0;
    int total_kw = kw_count + type_count;

    if (total_kw == 0) {
        lua_pop(L, 2);  /* pop types and keywords */
        return 0;  /* No keywords, not an error */
    }

    lang->keywords = calloc(total_kw + 1, sizeof(char*));
    if (!lang->keywords) {
        lua_pop(L, 2);
        lua_pushstring(L, "memory allocation failed for keywords");
        return -1;
    }

    /* Copy regular keywords */
    int idx = 0;
    if (kw_count > 0) {
        for (int i = 0; i < kw_count; i++) {
            lua_rawgeti(L, -2, i + 1);
            if (lua_isstring(L, -1)) {
                const char *kw = lua_tostring(L, -1);
                lang->keywords[idx] = strdup(kw);
                if (lang->keywords[idx]) {
                    idx++;
                }
            }
            lua_pop(L, 1);
        }
    }

    /* Copy type keywords (append "|" to distinguish them) */
    if (type_count > 0) {
        for (int i = 0; i < type_count; i++) {
            lua_rawgeti(L, -1, i + 1);
            if (lua_isstring(L, -1)) {
                const char *type = lua_tostring(L, -1);
                size_t len = strlen(type);
                char *type_with_pipe = malloc(len + 2);
                if (type_with_pipe) {
                    strcpy(type_with_pipe, type);
                    type_with_pipe[len] = '|';
                    type_with_pipe[len + 1] = '\0';
                    lang->keywords[idx++] = type_with_pipe;
                }
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 2);  /* pop types and keywords tables */
    return 0;
}

/* Extract and validate comment delimiters from Lua table
 * Returns 0 on success, -1 on error (error message pushed to stack) */
static int extract_comment_delimiters(lua_State *L, int table_idx, struct t_editor_syntax *lang) {
    /* Extract line comment */
    lua_getfield(L, table_idx, "line_comment");
    if (lua_isstring(L, -1)) {
        const char *lc = lua_tostring(L, -1);
        size_t len = strlen(lc);
        if (len >= sizeof(lang->singleline_comment_start)) {
            lua_pop(L, 1);
            lua_pushstring(L, "line_comment too long (max 3 chars)");
            return -1;
        }
        strncpy(lang->singleline_comment_start, lc, sizeof(lang->singleline_comment_start) - 1);
    }
    lua_pop(L, 1);

    /* Extract block comment start */
    lua_getfield(L, table_idx, "block_comment_start");
    if (lua_isstring(L, -1)) {
        const char *bcs = lua_tostring(L, -1);
        size_t len = strlen(bcs);
        if (len >= sizeof(lang->multiline_comment_start)) {
            lua_pop(L, 1);
            lua_pushstring(L, "block_comment_start too long (max 5 chars)");
            return -1;
        }
        strncpy(lang->multiline_comment_start, bcs, sizeof(lang->multiline_comment_start) - 1);
    }
    lua_pop(L, 1);

    /* Extract block comment end */
    lua_getfield(L, table_idx, "block_comment_end");
    if (lua_isstring(L, -1)) {
        const char *bce = lua_tostring(L, -1);
        size_t len = strlen(bce);
        if (len >= sizeof(lang->multiline_comment_end)) {
            lua_pop(L, 1);
            lua_pushstring(L, "block_comment_end too long (max 5 chars)");
            return -1;
        }
        strncpy(lang->multiline_comment_end, bce, sizeof(lang->multiline_comment_end) - 1);
    }
    lua_pop(L, 1);

    return 0;
}

/* Extract and validate separators from Lua table
 * Returns 0 on success, -1 on error (error message pushed to stack) */
static int extract_separators(lua_State *L, int table_idx, struct t_editor_syntax *lang) {
    lua_getfield(L, table_idx, "separators");
    if (lua_isstring(L, -1)) {
        const char *sep = lua_tostring(L, -1);
        lang->separators = strdup(sep);
        if (!lang->separators) {
            lua_pop(L, 1);
            lua_pushstring(L, "memory allocation failed for separators");
            return -1;
        }
    } else {
        /* Default separators */
        lang->separators = strdup(",.()+-/*=~%<>[];");
        if (!lang->separators) {
            lua_pop(L, 1);
            lua_pushstring(L, "memory allocation failed for default separators");
            return -1;
        }
    }
    lua_pop(L, 1);
    return 0;
}

/* Extract and validate highlight flags from Lua table
 * Always succeeds (uses defaults if fields not present) */
static void extract_highlight_flags(lua_State *L, int table_idx, struct t_editor_syntax *lang) {
    lua_getfield(L, table_idx, "highlight_strings");
    int hl_strings = lua_isboolean(L, -1) ? lua_toboolean(L, -1) : 1;  /* Default: true */
    lua_pop(L, 1);

    lua_getfield(L, table_idx, "highlight_numbers");
    int hl_numbers = lua_isboolean(L, -1) ? lua_toboolean(L, -1) : 1;  /* Default: true */
    lua_pop(L, 1);

    lang->flags = 0;
    if (hl_strings) lang->flags |= HL_HIGHLIGHT_STRINGS;
    if (hl_numbers) lang->flags |= HL_HIGHLIGHT_NUMBERS;
}

/* ============================================================================
 * Main Language Registration Function
 * ============================================================================ */

/* Lua API: loki.register_language(config) - Register a new language for syntax highlighting
 * config table must contain:
 *   - name (string): Language name
 *   - extensions (table): File extensions (e.g., {".py", ".pyw"})
 * Optional fields:
 *   - keywords (table): Language keywords
 *   - types (table): Type keywords
 *   - line_comment (string): Single-line comment delimiter
 *   - block_comment_start (string): Multi-line comment start
 *   - block_comment_end (string): Multi-line comment end
 *   - separators (string): Separator characters
 *   - highlight_strings (boolean): Enable string highlighting (default: true)
 *   - highlight_numbers (boolean): Enable number highlighting (default: true)
 */
static int lua_loki_register_language(lua_State *L) {
    /* Validate argument is a table */
    if (!lua_istable(L, 1)) {
        lua_pushnil(L);
        lua_pushstring(L, "argument must be a table");
        return 2;
    }

    /* Allocate new language structure */
    struct t_editor_syntax *lang = calloc(1, sizeof(struct t_editor_syntax));
    if (!lang) {
        lua_pushnil(L);
        lua_pushstring(L, "memory allocation failed");
        return 2;
    }

    /* Extract name (required - just for validation, not stored) */
    lua_getfield(L, 1, "name");
    if (!lua_isstring(L, -1)) {
        free(lang);
        lua_pushnil(L);
        lua_pushstring(L, "'name' field is required and must be a string");
        return 2;
    }
    lua_pop(L, 1);

    /* Extract extensions using helper */
    if (extract_language_extensions(L, 1, lang) != 0) {
        free_dynamic_language(lang);
        /* Error message already on stack, insert nil below it */
        lua_pushnil(L);
        lua_insert(L, -2);  /* Move nil below error message */
        return 2;
    }

    /* Extract keywords using helper */
    if (extract_language_keywords(L, 1, lang) != 0) {
        free_dynamic_language(lang);
        /* Error message already on stack, insert nil below it */
        lua_pushnil(L);
        lua_insert(L, -2);
        return 2;
    }

    /* Extract comment delimiters using helper */
    if (extract_comment_delimiters(L, 1, lang) != 0) {
        free_dynamic_language(lang);
        /* Error message already on stack, insert nil below it */
        lua_pushnil(L);
        lua_insert(L, -2);
        return 2;
    }

    /* Extract separators using helper */
    if (extract_separators(L, 1, lang) != 0) {
        free_dynamic_language(lang);
        /* Error message already on stack, insert nil below it */
        lua_pushnil(L);
        lua_insert(L, -2);
        return 2;
    }

    /* Extract highlight flags using helper */
    extract_highlight_flags(L, 1, lang);

    /* Set highlighting type */
    lang->type = HL_TYPE_C;  /* Use C-style highlighting */

    /* Add to dynamic registry */
    if (add_dynamic_language(lang) != 0) {
        free_dynamic_language(lang);
        lua_pushnil(L);
        lua_pushstring(L, "failed to register language");
        return 2;
    }

    lua_pushboolean(L, 1);  /* Success */
    return 1;
}

static int lua_loki_status_stdout(lua_State *L) {
    const char *msg = luaL_checkstring(L, 1);
    if (msg && *msg) {
        fprintf(stdout, "[loki] %s\n", msg);
        fflush(stdout);
    }
    return 0;
}

/* ======================= Keybinding System ================================== */

/* Parse key notation string to key code
 * Supports:
 *   - Single characters: 'a', 'x', ':'
 *   - Control keys: '<C-a>', '<C-s>'
 *   - Special keys: '<Enter>', '<Esc>', '<Tab>', '<BS>'
 *   - Arrow keys: '<Up>', '<Down>', '<Left>', '<Right>'
 *   - Other: '<Home>', '<End>', '<PageUp>', '<PageDown>', '<Del>'
 * Returns -1 on parse error */
static int parse_key_notation(const char *notation) {
    if (!notation || !*notation) return -1;

    /* Single character */
    if (notation[1] == '\0') {
        return (unsigned char)notation[0];
    }

    /* Special key notation: <...> */
    if (notation[0] != '<') return -1;

    size_t len = strlen(notation);
    if (len < 3 || notation[len-1] != '>') return -1;

    /* Control key: <C-x> */
    if (len == 5 && notation[1] == 'C' && notation[2] == '-') {
        char c = notation[3];
        if (c >= 'a' && c <= 'z') {
            return c - 'a' + 1;  /* Ctrl-a = 1, Ctrl-z = 26 */
        }
        if (c >= 'A' && c <= 'Z') {
            return c - 'A' + 1;
        }
        return -1;
    }

    /* Named special keys (case-insensitive comparison) */
    const char *name = notation + 1;  /* Skip '<' */

    if (strcasecmp(name, "Enter>") == 0 || strcasecmp(name, "CR>") == 0 ||
        strcasecmp(name, "Return>") == 0) return ENTER;
    if (strcasecmp(name, "Esc>") == 0 || strcasecmp(name, "Escape>") == 0) return ESC;
    if (strcasecmp(name, "Tab>") == 0) return TAB;
    if (strcasecmp(name, "BS>") == 0 || strcasecmp(name, "Backspace>") == 0) return BACKSPACE;
    if (strcasecmp(name, "Up>") == 0) return ARROW_UP;
    if (strcasecmp(name, "Down>") == 0) return ARROW_DOWN;
    if (strcasecmp(name, "Left>") == 0) return ARROW_LEFT;
    if (strcasecmp(name, "Right>") == 0) return ARROW_RIGHT;
    if (strcasecmp(name, "Home>") == 0) return HOME_KEY;
    if (strcasecmp(name, "End>") == 0) return END_KEY;
    if (strcasecmp(name, "PageUp>") == 0) return PAGE_UP;
    if (strcasecmp(name, "PageDown>") == 0) return PAGE_DOWN;
    if (strcasecmp(name, "Del>") == 0 || strcasecmp(name, "Delete>") == 0) return DEL_KEY;
    if (strcasecmp(name, "S-Up>") == 0) return SHIFT_ARROW_UP;
    if (strcasecmp(name, "S-Down>") == 0) return SHIFT_ARROW_DOWN;
    if (strcasecmp(name, "S-Left>") == 0) return SHIFT_ARROW_LEFT;
    if (strcasecmp(name, "S-Right>") == 0) return SHIFT_ARROW_RIGHT;
    if (strcasecmp(name, "S-Return>") == 0 || strcasecmp(name, "S-Enter>") == 0) return SHIFT_RETURN;

    return -1;
}

/* Convert mode character to mode table name */
static const char *mode_char_to_name(char mode) {
    switch (mode) {
        case 'n': return "normal";
        case 'i': return "insert";
        case 'v': return "visual";
        case 'c': return "command";
        default: return NULL;
    }
}

/* Ensure _loki_keymaps table and mode subtable exist */
static void ensure_keymap_tables(lua_State *L, const char *mode_name) {
    /* Get or create _loki_keymaps */
    lua_getglobal(L, "_loki_keymaps");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "_loki_keymaps");
    }

    /* Get or create mode subtable */
    lua_getfield(L, -1, mode_name);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, mode_name);
    }
    /* Stack: _loki_keymaps, mode_table */
}

/* Lua API: loki.keymap(modes, key, callback, [description])
 * Register a keybinding for one or more modes
 *
 * modes: string of mode characters ('n', 'i', 'v', 'c')
 * key: key notation string (e.g., 'a', '<C-s>', '<Enter>')
 * callback: Lua function to call when key is pressed
 * description: optional description for help
 *
 * Example:
 *   loki.keymap('n', 'gd', function() loki.status("Go to definition") end)
 *   loki.keymap('nv', '<C-y>', function() ... end, "Copy line")
 */
static int lua_loki_keymap(lua_State *L) {
    const char *modes = luaL_checkstring(L, 1);
    const char *key_notation = luaL_checkstring(L, 2);
    luaL_checktype(L, 3, LUA_TFUNCTION);
    const char *description = luaL_optstring(L, 4, NULL);

    /* Parse key notation to key code */
    int keycode = parse_key_notation(key_notation);
    if (keycode < 0) {
        return luaL_error(L, "Invalid key notation: %s", key_notation);
    }

    /* Register for each mode character */
    for (const char *m = modes; *m; m++) {
        const char *mode_name = mode_char_to_name(*m);
        if (!mode_name) {
            return luaL_error(L, "Invalid mode character: %c (use n/i/v/c)", *m);
        }

        ensure_keymap_tables(L, mode_name);
        /* Stack: _loki_keymaps, mode_table */

        /* Store callback: mode_table[keycode] = callback */
        lua_pushinteger(L, keycode);
        lua_pushvalue(L, 3);  /* Push callback function */
        lua_settable(L, -3);

        /* Store description if provided: mode_table["desc_" .. keycode] = description */
        if (description) {
            char desc_key[32];
            snprintf(desc_key, sizeof(desc_key), "desc_%d", keycode);
            lua_pushstring(L, description);
            lua_setfield(L, -2, desc_key);
        }

        lua_pop(L, 2);  /* Pop mode_table and _loki_keymaps */
    }

    return 0;
}

/* Lua API: loki.keyunmap(modes, key)
 * Remove a keybinding for one or more modes
 *
 * modes: string of mode characters ('n', 'i', 'v', 'c')
 * key: key notation string
 */
static int lua_loki_keyunmap(lua_State *L) {
    const char *modes = luaL_checkstring(L, 1);
    const char *key_notation = luaL_checkstring(L, 2);

    int keycode = parse_key_notation(key_notation);
    if (keycode < 0) {
        return luaL_error(L, "Invalid key notation: %s", key_notation);
    }

    for (const char *m = modes; *m; m++) {
        const char *mode_name = mode_char_to_name(*m);
        if (!mode_name) continue;

        lua_getglobal(L, "_loki_keymaps");
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            continue;
        }

        lua_getfield(L, -1, mode_name);
        if (!lua_istable(L, -1)) {
            lua_pop(L, 2);
            continue;
        }

        /* Set mode_table[keycode] = nil */
        lua_pushinteger(L, keycode);
        lua_pushnil(L);
        lua_settable(L, -3);

        /* Also remove description */
        char desc_key[32];
        snprintf(desc_key, sizeof(desc_key), "desc_%d", keycode);
        lua_pushnil(L);
        lua_setfield(L, -2, desc_key);

        lua_pop(L, 2);
    }

    return 0;
}



/* ======================= Ableton Link Bindings ======================= */

/* Lua API: loki.link.init(bpm) - Initialize Link subsystem */
static int lua_link_init(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) {
        lua_pushnil(L);
        lua_pushstring(L, "No editor context");
        return 2;
    }

    double bpm = 120.0;
    if (lua_gettop(L) >= 1 && lua_isnumber(L, 1)) {
        bpm = lua_tonumber(L, 1);
    }

    int result = loki_link_init(ctx, bpm);
    if (result != 0) {
        lua_pushnil(L);
        lua_pushstring(L, "Failed to initialize Link");
        return 2;
    }

    lua_pushboolean(L, 1);
    return 1;
}

/* Lua API: loki.link.cleanup() - Cleanup Link subsystem */
static int lua_link_cleanup(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) return 0;

    loki_link_cleanup(ctx);
    return 0;
}

/* Lua API: loki.link.is_initialized() - Check if Link is initialized */
static int lua_link_is_initialized(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, loki_link_is_initialized(ctx));
    return 1;
}

/* Lua API: loki.link.enable(bool) - Enable/disable Link */
static int lua_link_enable(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) return 0;

    int enable = lua_toboolean(L, 1);
    loki_link_enable(ctx, enable);
    return 0;
}

/* Lua API: loki.link.is_enabled() - Check if Link is enabled */
static int lua_link_is_enabled(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, loki_link_is_enabled(ctx));
    return 1;
}

/* Lua API: loki.link.tempo() - Get session tempo */
static int lua_link_tempo(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) {
        lua_pushnumber(L, 120.0);  /* Default tempo */
        return 1;
    }

    lua_pushnumber(L, loki_link_get_tempo(ctx));
    return 1;
}

/* Lua API: loki.link.set_tempo(bpm) - Set session tempo */
static int lua_link_set_tempo(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) return 0;

    double bpm = luaL_checknumber(L, 1);
    loki_link_set_tempo(ctx, bpm);
    return 0;
}

/* Lua API: loki.link.beat(quantum) - Get current beat position */
static int lua_link_beat(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) {
        lua_pushnumber(L, 0.0);
        return 1;
    }

    double quantum = 4.0;
    if (lua_gettop(L) >= 1 && lua_isnumber(L, 1)) {
        quantum = lua_tonumber(L, 1);
    }

    lua_pushnumber(L, loki_link_get_beat(ctx, quantum));
    return 1;
}

/* Lua API: loki.link.phase(quantum) - Get phase within quantum */
static int lua_link_phase(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) {
        lua_pushnumber(L, 0.0);
        return 1;
    }

    double quantum = 4.0;
    if (lua_gettop(L) >= 1 && lua_isnumber(L, 1)) {
        quantum = lua_tonumber(L, 1);
    }

    lua_pushnumber(L, loki_link_get_phase(ctx, quantum));
    return 1;
}

/* Lua API: loki.link.peers() - Get number of connected peers */
static int lua_link_peers(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) {
        lua_pushinteger(L, 0);
        return 1;
    }

    lua_pushinteger(L, (lua_Integer)loki_link_num_peers(ctx));
    return 1;
}

/* Lua API: loki.link.start_stop_sync(bool) - Enable start/stop sync */
static int lua_link_start_stop_sync(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) return 0;

    int enable = lua_toboolean(L, 1);
    loki_link_enable_start_stop_sync(ctx, enable);
    return 0;
}

/* Lua API: loki.link.is_start_stop_sync_enabled() */
static int lua_link_is_start_stop_sync_enabled(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, loki_link_is_start_stop_sync_enabled(ctx));
    return 1;
}

/* Lua API: loki.link.is_playing() - Get transport state */
static int lua_link_is_playing(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, loki_link_is_playing(ctx));
    return 1;
}

/* Lua API: loki.link.play() - Start transport */
static int lua_link_play(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) return 0;

    loki_link_set_playing(ctx, 1);
    return 0;
}

/* Lua API: loki.link.stop() - Stop transport */
static int lua_link_stop(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) return 0;

    loki_link_set_playing(ctx, 0);
    return 0;
}

/* Lua API: loki.link.on_peers(callback) - Register peers callback */
static int lua_link_on_peers(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) return 0;

    const char *callback = NULL;
    if (lua_gettop(L) >= 1 && !lua_isnil(L, 1)) {
        callback = luaL_checkstring(L, 1);
    }

    loki_link_set_peers_callback(ctx, callback);
    return 0;
}

/* Lua API: loki.link.on_tempo(callback) - Register tempo callback */
static int lua_link_on_tempo(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) return 0;

    const char *callback = NULL;
    if (lua_gettop(L) >= 1 && !lua_isnil(L, 1)) {
        callback = luaL_checkstring(L, 1);
    }

    loki_link_set_tempo_callback(ctx, callback);
    return 0;
}

/* Lua API: loki.link.on_start_stop(callback) - Register start/stop callback */
static int lua_link_on_start_stop(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) return 0;

    const char *callback = NULL;
    if (lua_gettop(L) >= 1 && !lua_isnil(L, 1)) {
        callback = luaL_checkstring(L, 1);
    }

    loki_link_set_start_stop_callback(ctx, callback);
    return 0;
}

/* Lua API: loki.link.launch_quantize(quantum) - Set launch quantization
 * quantum: 0 = immediate, 1 = next beat, 4 = next bar (4/4), etc.
 * Returns current value if called with no args */
static int lua_link_launch_quantize(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushnil(L);
        return 1;
    }

    if (lua_gettop(L) >= 1) {
        /* Set mode */
        int quantum = (int)luaL_checkinteger(L, 1);
        if (quantum < 0) quantum = 0;
        ctx->model.shared->launch_quantize = quantum;
    }

    /* Return current value */
    lua_pushinteger(L, ctx->model.shared->launch_quantize);
    return 1;
}

/* Register link module as loki.link subtable */
static void lua_register_link_module(lua_State *L) {
    /* Assumes loki table is on top of stack */
    lua_newtable(L);  /* Create link subtable */

    lua_pushcfunction(L, lua_link_init);
    lua_setfield(L, -2, "init");

    lua_pushcfunction(L, lua_link_cleanup);
    lua_setfield(L, -2, "cleanup");

    lua_pushcfunction(L, lua_link_is_initialized);
    lua_setfield(L, -2, "is_initialized");

    lua_pushcfunction(L, lua_link_enable);
    lua_setfield(L, -2, "enable");

    lua_pushcfunction(L, lua_link_is_enabled);
    lua_setfield(L, -2, "is_enabled");

    lua_pushcfunction(L, lua_link_tempo);
    lua_setfield(L, -2, "tempo");

    lua_pushcfunction(L, lua_link_set_tempo);
    lua_setfield(L, -2, "set_tempo");

    lua_pushcfunction(L, lua_link_beat);
    lua_setfield(L, -2, "beat");

    lua_pushcfunction(L, lua_link_phase);
    lua_setfield(L, -2, "phase");

    lua_pushcfunction(L, lua_link_peers);
    lua_setfield(L, -2, "peers");

    lua_pushcfunction(L, lua_link_start_stop_sync);
    lua_setfield(L, -2, "start_stop_sync");

    lua_pushcfunction(L, lua_link_is_start_stop_sync_enabled);
    lua_setfield(L, -2, "is_start_stop_sync_enabled");

    lua_pushcfunction(L, lua_link_is_playing);
    lua_setfield(L, -2, "is_playing");

    lua_pushcfunction(L, lua_link_play);
    lua_setfield(L, -2, "play");

    lua_pushcfunction(L, lua_link_stop);
    lua_setfield(L, -2, "stop");

    lua_pushcfunction(L, lua_link_on_peers);
    lua_setfield(L, -2, "on_peers");

    lua_pushcfunction(L, lua_link_on_tempo);
    lua_setfield(L, -2, "on_tempo");

    lua_pushcfunction(L, lua_link_on_start_stop);
    lua_setfield(L, -2, "on_start_stop");

    lua_pushcfunction(L, lua_link_launch_quantize);
    lua_setfield(L, -2, "launch_quantize");

    lua_setfield(L, -2, "link");  /* Set as loki.link */
}

/* ======================= MIDI Port Lua Bindings ======================= */

/* Lua API: loki.midi.port_count() - Get number of available MIDI output ports */
static int lua_midi_port_count(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushinteger(L, 0);
        return 1;
    }
    lua_pushinteger(L, shared_midi_get_port_count(ctx->model.shared));
    return 1;
}

/* Lua API: loki.midi.port_name(index) - Get name of MIDI port at index (1-based) */
static int lua_midi_port_name(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushnil(L);
        return 1;
    }
    int idx = (int)luaL_checkinteger(L, 1) - 1;  /* Convert to 0-based */
    const char *name = shared_midi_get_port_name(ctx->model.shared, idx);
    if (name) {
        lua_pushstring(L, name);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

/* Lua API: loki.midi.list_ports() - Get table of all MIDI port names */
static int lua_midi_list_ports(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_newtable(L);
        return 1;
    }
    int count = shared_midi_get_port_count(ctx->model.shared);
    lua_createtable(L, count, 0);
    for (int i = 0; i < count; i++) {
        const char *name = shared_midi_get_port_name(ctx->model.shared, i);
        if (name) {
            lua_pushstring(L, name);
            lua_rawseti(L, -2, i + 1);  /* 1-based Lua index */
        }
    }
    return 1;
}

/* Lua API: loki.midi.open_port(index) - Open MIDI port by index (1-based) */
static int lua_midi_open_port(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "No shared context");
        return 2;
    }
    int idx = (int)luaL_checkinteger(L, 1) - 1;  /* Convert to 0-based */
    if (shared_midi_open_port(ctx->model.shared, idx) == 0) {
        lua_pushboolean(L, 1);
        return 1;
    } else {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Failed to open port");
        return 2;
    }
}

/* Lua API: loki.midi.open_by_name(name) - Open MIDI port by name (substring match) */
static int lua_midi_open_by_name(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "No shared context");
        return 2;
    }
    const char *name = luaL_checkstring(L, 1);
    if (shared_midi_open_by_name(ctx->model.shared, name) == 0) {
        lua_pushboolean(L, 1);
        return 1;
    } else {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "No port matching name");
        return 2;
    }
}

/* Lua API: loki.midi.open_virtual(name) - Create virtual MIDI port */
static int lua_midi_open_virtual(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "No shared context");
        return 2;
    }
    const char *name = luaL_optstring(L, 1, "PSND_MIDI");
    if (shared_midi_open_virtual(ctx->model.shared, name) == 0) {
        lua_pushboolean(L, 1);
        return 1;
    } else {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Failed to create virtual port");
        return 2;
    }
}

/* Lua API: loki.midi.close() - Close current MIDI port */
static int lua_midi_close(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        return 0;
    }
    shared_midi_close(ctx->model.shared);
    return 0;
}

/* Lua API: loki.midi.is_open() - Check if MIDI port is open */
static int lua_midi_is_open(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushboolean(L, 0);
        return 1;
    }
    lua_pushboolean(L, shared_midi_is_open(ctx->model.shared));
    return 1;
}

/* ======================= MIDI Export Lua Bindings ======================= */

/* Lua API: loki.midi.export(filename) - Export events to MIDI file
 * Returns: true on success, nil + error message on failure */
static int lua_midi_export(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx) {
        lua_pushnil(L);
        lua_pushstring(L, "No editor context");
        return 2;
    }

    const char *filename = luaL_checkstring(L, 1);
    if (loki_export_midi(ctx, filename) == 0) {
        lua_pushboolean(L, 1);
        return 1;
    } else {
        lua_pushnil(L);
        const char *err = loki_export_error();
        lua_pushstring(L, err ? err : "unknown error");
        return 2;
    }
}

/* ======================= OSC Lua Bindings ======================= */

/* Lua API: loki.osc.init(port) - Initialize OSC on specified port
 * port is optional, defaults to 7770 */
static int lua_osc_init(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "No shared context");
        return 2;
    }

    int port = (int)luaL_optinteger(L, 1, PSND_OSC_DEFAULT_PORT);

    if (shared_osc_init(ctx->model.shared, port) == 0) {
        lua_pushboolean(L, 1);
        return 1;
    } else {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Failed to initialize OSC");
        return 2;
    }
}

/* Lua API: loki.osc.start() - Start OSC server */
static int lua_osc_start(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "No shared context");
        return 2;
    }

    if (shared_osc_start(ctx->model.shared) == 0) {
        lua_pushboolean(L, 1);
        return 1;
    } else {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Failed to start OSC server");
        return 2;
    }
}

/* Lua API: loki.osc.stop() - Stop OSC server */
static int lua_osc_stop(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        return 0;
    }

    shared_osc_cleanup(ctx->model.shared);
    return 0;
}

/* Lua API: loki.osc.enabled() - Check if OSC is running */
static int lua_osc_enabled(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, shared_osc_is_running(ctx->model.shared));
    return 1;
}

/* Lua API: loki.osc.port() - Get OSC port number */
static int lua_osc_port(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushinteger(L, 0);
        return 1;
    }

    lua_pushinteger(L, shared_osc_get_port(ctx->model.shared));
    return 1;
}

/* Lua API: loki.osc.broadcast(host, port) - Set broadcast target */
static int lua_osc_broadcast(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "No shared context");
        return 2;
    }

    const char *host = luaL_checkstring(L, 1);
    const char *port;

    /* Port can be number or string */
    if (lua_isnumber(L, 2)) {
        char port_buf[16];
        snprintf(port_buf, sizeof(port_buf), "%d", (int)lua_tointeger(L, 2));
        port = port_buf;
        if (shared_osc_set_broadcast(ctx->model.shared, host, port) == 0) {
            lua_pushboolean(L, 1);
            return 1;
        }
    } else {
        port = luaL_checkstring(L, 2);
        if (shared_osc_set_broadcast(ctx->model.shared, host, port) == 0) {
            lua_pushboolean(L, 1);
            return 1;
        }
    }

    lua_pushboolean(L, 0);
    lua_pushstring(L, "Failed to set broadcast target");
    return 2;
}

/* Helper to build lo_message from Lua stack arguments */
#ifdef PSND_OSC
static lo_message lua_build_osc_message(lua_State *L, int start_idx) {
    lo_message msg = lo_message_new();
    if (!msg) return NULL;

    int nargs = lua_gettop(L);
    for (int i = start_idx; i <= nargs; i++) {
        int type = lua_type(L, i);
        switch (type) {
            case LUA_TNUMBER:
                if (lua_isinteger(L, i)) {
                    lo_message_add_int32(msg, (int32_t)lua_tointeger(L, i));
                } else {
                    lo_message_add_float(msg, (float)lua_tonumber(L, i));
                }
                break;
            case LUA_TSTRING:
                lo_message_add_string(msg, lua_tostring(L, i));
                break;
            case LUA_TBOOLEAN:
                if (lua_toboolean(L, i)) {
                    lo_message_add_true(msg);
                } else {
                    lo_message_add_false(msg);
                }
                break;
            case LUA_TNIL:
                lo_message_add_nil(msg);
                break;
            default:
                /* Skip unsupported types */
                break;
        }
    }
    return msg;
}
#endif

/* Lua API: loki.osc.send(path, ...) - Send OSC message to broadcast target */
static int lua_osc_send(lua_State *L) {
#ifdef PSND_OSC
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "No shared context");
        return 2;
    }

    if (!ctx->model.shared->osc_broadcast) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "No broadcast target set");
        return 2;
    }

    const char *path = luaL_checkstring(L, 1);

    lo_message msg = lua_build_osc_message(L, 2);
    if (!msg) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Failed to create message");
        return 2;
    }

    int result = lo_send_message(ctx->model.shared->osc_broadcast, path, msg);
    lo_message_free(msg);

    if (result >= 0) {
        lua_pushboolean(L, 1);
        return 1;
    } else {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Failed to send message");
        return 2;
    }
#else
    (void)L;
    lua_pushboolean(L, 0);
    lua_pushstring(L, "OSC not available");
    return 2;
#endif
}

/* Lua API: loki.osc.send_to(host, port, path, ...) - Send OSC message to specific address */
static int lua_osc_send_to(lua_State *L) {
#ifdef PSND_OSC
    const char *host = luaL_checkstring(L, 1);
    char port_buf[16];
    const char *port;

    /* Port can be number or string */
    if (lua_isnumber(L, 2)) {
        snprintf(port_buf, sizeof(port_buf), "%d", (int)lua_tointeger(L, 2));
        port = port_buf;
    } else {
        port = luaL_checkstring(L, 2);
    }

    const char *path = luaL_checkstring(L, 3);

    lo_address addr = lo_address_new(host, port);
    if (!addr) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Failed to create address");
        return 2;
    }

    lo_message msg = lua_build_osc_message(L, 4);
    if (!msg) {
        lo_address_free(addr);
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Failed to create message");
        return 2;
    }

    int result = lo_send_message(addr, path, msg);
    lo_message_free(msg);
    lo_address_free(addr);

    if (result >= 0) {
        lua_pushboolean(L, 1);
        return 1;
    } else {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "Failed to send message");
        return 2;
    }
#else
    (void)L;
    lua_pushboolean(L, 0);
    lua_pushstring(L, "OSC not available");
    return 2;
#endif
}

#ifdef PSND_OSC
/* Registry key for storing OSC Lua callbacks */
static const char osc_callbacks_registry_key = 0;
#endif

/* Lua API: loki.osc.on(path, callback_name) - Register callback for OSC path
 * callback_name is the name of a global Lua function to call when message arrives
 * Note: Due to thread safety, the callback is invoked by name from the main thread */
static int lua_osc_on(lua_State *L) {
#ifdef PSND_OSC
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushboolean(L, 0);
        lua_pushstring(L, "No shared context");
        return 2;
    }

    const char *path = luaL_checkstring(L, 1);
    const char *callback = luaL_checkstring(L, 2);

    /* Store callback in registry table: callbacks[path] = callback_name */
    lua_pushlightuserdata(L, (void *)&osc_callbacks_registry_key);
    lua_gettable(L, LUA_REGISTRYINDEX);
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushlightuserdata(L, (void *)&osc_callbacks_registry_key);
        lua_pushvalue(L, -2);
        lua_settable(L, LUA_REGISTRYINDEX);
    }
    lua_pushstring(L, callback);
    lua_setfield(L, -2, path);
    lua_pop(L, 1);

    lua_pushboolean(L, 1);
    return 1;
#else
    (void)L;
    lua_pushboolean(L, 0);
    lua_pushstring(L, "OSC not available");
    return 2;
#endif
}

/* Lua API: loki.osc.off(path) - Remove callback for OSC path */
static int lua_osc_off(lua_State *L) {
#ifdef PSND_OSC
    const char *path = luaL_checkstring(L, 1);

    /* Remove from registry table */
    lua_pushlightuserdata(L, (void *)&osc_callbacks_registry_key);
    lua_gettable(L, LUA_REGISTRYINDEX);
    if (!lua_isnil(L, -1)) {
        lua_pushnil(L);
        lua_setfield(L, -2, path);
    }
    lua_pop(L, 1);

    return 0;
#else
    (void)L;
    return 0;
#endif
}

/* Lua API: loki.osc.rate_limit(msgs_per_sec) - Set/get note rate limit
 * If called with argument, sets the limit. Always returns current limit.
 * 0 = unlimited */
static int lua_osc_rate_limit(lua_State *L) {
    if (lua_gettop(L) >= 1) {
        int limit = (int)luaL_checkinteger(L, 1);
        shared_osc_set_note_rate_limit(limit);
    }
    lua_pushinteger(L, shared_osc_get_note_rate_limit());
    return 1;
}

/* Register osc module as loki.osc subtable */
static void lua_register_osc_module(lua_State *L) {
    /* Assumes loki table is on top of stack */
    lua_newtable(L);  /* Create osc subtable */

    lua_pushcfunction(L, lua_osc_init);
    lua_setfield(L, -2, "init");

    lua_pushcfunction(L, lua_osc_start);
    lua_setfield(L, -2, "start");

    lua_pushcfunction(L, lua_osc_stop);
    lua_setfield(L, -2, "stop");

    lua_pushcfunction(L, lua_osc_enabled);
    lua_setfield(L, -2, "enabled");

    lua_pushcfunction(L, lua_osc_port);
    lua_setfield(L, -2, "port");

    lua_pushcfunction(L, lua_osc_broadcast);
    lua_setfield(L, -2, "broadcast");

    lua_pushcfunction(L, lua_osc_send);
    lua_setfield(L, -2, "send");

    lua_pushcfunction(L, lua_osc_send_to);
    lua_setfield(L, -2, "send_to");

    lua_pushcfunction(L, lua_osc_on);
    lua_setfield(L, -2, "on");

    lua_pushcfunction(L, lua_osc_off);
    lua_setfield(L, -2, "off");

    lua_pushcfunction(L, lua_osc_rate_limit);
    lua_setfield(L, -2, "rate_limit");

    lua_setfield(L, -2, "osc");  /* Set as loki.osc */
}

/* ======================= Parameter Lua Bindings ======================= */

/* Lua API: loki.param.define(name, opts) - Define a new parameter
 * opts: { type="float"|"int"|"bool", min=0, max=1, default=0.5 }
 * Returns parameter index on success, nil + error on failure */
static int lua_param_define(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushnil(L);
        lua_pushstring(L, "no shared context");
        return 2;
    }

    const char *name = luaL_checkstring(L, 1);
    luaL_checktype(L, 2, LUA_TTABLE);

    /* Extract type (default: float) */
    ParamType type = PARAM_TYPE_FLOAT;
    lua_getfield(L, 2, "type");
    if (lua_isstring(L, -1)) {
        const char *type_str = lua_tostring(L, -1);
        if (strcmp(type_str, "int") == 0 || strcmp(type_str, "integer") == 0) {
            type = PARAM_TYPE_INT;
        } else if (strcmp(type_str, "bool") == 0 || strcmp(type_str, "boolean") == 0) {
            type = PARAM_TYPE_BOOL;
        }
    }
    lua_pop(L, 1);

    /* Extract min (default: 0) */
    float min_val = 0.0f;
    lua_getfield(L, 2, "min");
    if (lua_isnumber(L, -1)) {
        min_val = (float)lua_tonumber(L, -1);
    }
    lua_pop(L, 1);

    /* Extract max (default: 1) */
    float max_val = 1.0f;
    lua_getfield(L, 2, "max");
    if (lua_isnumber(L, -1)) {
        max_val = (float)lua_tonumber(L, -1);
    }
    lua_pop(L, 1);

    /* Extract default (default: min) */
    float def_val = min_val;
    lua_getfield(L, 2, "default");
    if (lua_isnumber(L, -1)) {
        def_val = (float)lua_tonumber(L, -1);
    }
    lua_pop(L, 1);

    int idx = shared_param_define(ctx->model.shared, name, type, min_val, max_val, def_val);
    if (idx < 0) {
        lua_pushnil(L);
        lua_pushstring(L, "failed to define parameter (exists or store full)");
        return 2;
    }

    lua_pushinteger(L, idx);
    return 1;
}

/* Lua API: loki.param.get(name) - Get parameter value */
static int lua_param_get(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushnil(L);
        return 1;
    }

    const char *name = luaL_checkstring(L, 1);
    float value;
    if (shared_param_get(ctx->model.shared, name, &value) == 0) {
        lua_pushnumber(L, value);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

/* Lua API: loki.param.set(name, value) - Set parameter value */
static int lua_param_set(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const char *name = luaL_checkstring(L, 1);
    float value = (float)luaL_checknumber(L, 2);

    int result = shared_param_set(ctx->model.shared, name, value);
    lua_pushboolean(L, result == 0);
    return 1;
}

/* Lua API: loki.param.reset(name) - Reset parameter to default */
static int lua_param_reset(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const char *name = luaL_checkstring(L, 1);
    int result = shared_param_reset(ctx->model.shared, name);
    lua_pushboolean(L, result == 0);
    return 1;
}

/* Lua API: loki.param.bind_osc(name, path) - Bind parameter to OSC path */
static int lua_param_bind_osc(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const char *name = luaL_checkstring(L, 1);
    const char *path = luaL_checkstring(L, 2);

    int result = shared_param_bind_osc(ctx->model.shared, name, path);
    lua_pushboolean(L, result == 0);
    return 1;
}

/* Lua API: loki.param.unbind_osc(name) - Unbind parameter from OSC */
static int lua_param_unbind_osc(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const char *name = luaL_checkstring(L, 1);
    int result = shared_param_unbind_osc(ctx->model.shared, name);
    lua_pushboolean(L, result == 0);
    return 1;
}

/* Lua API: loki.param.bind_midi(name, channel, cc) - Bind parameter to MIDI CC */
static int lua_param_bind_midi(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const char *name = luaL_checkstring(L, 1);
    int channel = (int)luaL_checkinteger(L, 2);
    int cc = (int)luaL_checkinteger(L, 3);

    int result = shared_param_bind_midi_cc(ctx->model.shared, name, channel, cc);
    lua_pushboolean(L, result == 0);
    return 1;
}

/* Lua API: loki.param.unbind_midi(name) - Unbind parameter from MIDI CC */
static int lua_param_unbind_midi(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const char *name = luaL_checkstring(L, 1);
    int result = shared_param_unbind_midi_cc(ctx->model.shared, name);
    lua_pushboolean(L, result == 0);
    return 1;
}

/* Lua API: loki.param.undefine(name) - Remove parameter definition */
static int lua_param_undefine(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const char *name = luaL_checkstring(L, 1);
    int result = shared_param_undefine(ctx->model.shared, name);
    lua_pushboolean(L, result == 0);
    return 1;
}

/* Lua API: loki.param.list() - Get table of all defined parameters */
static int lua_param_list(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_newtable(L);
        return 1;
    }

    lua_newtable(L);
    int list_idx = 1;

    for (int i = 0; i < PARAM_MAX_COUNT; i++) {
        const SharedParam *p = shared_param_at(ctx->model.shared, i);
        if (p) {
            lua_newtable(L);

            lua_pushstring(L, p->name);
            lua_setfield(L, -2, "name");

            lua_pushnumber(L, shared_param_get_idx(ctx->model.shared, i));
            lua_setfield(L, -2, "value");

            lua_pushnumber(L, p->min_val);
            lua_setfield(L, -2, "min");

            lua_pushnumber(L, p->max_val);
            lua_setfield(L, -2, "max");

            lua_pushnumber(L, p->default_val);
            lua_setfield(L, -2, "default");

            if (p->osc_path[0]) {
                lua_pushstring(L, p->osc_path);
                lua_setfield(L, -2, "osc_path");
            }

            if (p->midi_channel > 0) {
                lua_pushinteger(L, p->midi_channel);
                lua_setfield(L, -2, "midi_channel");
                lua_pushinteger(L, p->midi_cc);
                lua_setfield(L, -2, "midi_cc");
            }

            lua_rawseti(L, -2, list_idx++);
        }
    }

    return 1;
}

/* Lua API: loki.param.count() - Get number of defined parameters */
static int lua_param_count(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushinteger(L, 0);
        return 1;
    }

    lua_pushinteger(L, shared_param_count(ctx->model.shared));
    return 1;
}

/* Lua API: loki.param.reset_all() - Reset all parameters to defaults */
static int lua_param_reset_all(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        return 0;
    }

    shared_param_reset_all(ctx->model.shared);
    return 0;
}

/* Lua API: loki.param.midi_in_open(port_idx) - Open MIDI input port for CC binding */
static int lua_param_midi_in_open(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_pushboolean(L, 0);
        return 1;
    }

    int port_idx = (int)luaL_checkinteger(L, 1) - 1;  /* Convert to 0-based */
    int result = shared_midi_in_open_port(ctx->model.shared, port_idx);
    lua_pushboolean(L, result == 0);
    return 1;
}

/* Lua API: loki.param.midi_in_close() - Close MIDI input port */
static int lua_param_midi_in_close(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        return 0;
    }

    shared_midi_in_close(ctx->model.shared);
    return 0;
}

/* Lua API: loki.param.midi_in_list() - List available MIDI input ports */
static int lua_param_midi_in_list(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (!ctx || !ctx->model.shared) {
        lua_newtable(L);
        return 1;
    }

    shared_midi_in_init_observer(ctx->model.shared);

    lua_newtable(L);
    int count = shared_midi_in_get_port_count(ctx->model.shared);
    for (int i = 0; i < count; i++) {
        const char *name = shared_midi_in_get_port_name(ctx->model.shared, i);
        if (name) {
            lua_pushstring(L, name);
            lua_rawseti(L, -2, i + 1);  /* 1-based index */
        }
    }

    return 1;
}

/* Register param module as loki.param subtable */
static void lua_register_param_module(lua_State *L) {
    /* Assumes loki table is on top of stack */
    lua_newtable(L);  /* Create param subtable */

    /* Core operations */
    lua_pushcfunction(L, lua_param_define);
    lua_setfield(L, -2, "define");

    lua_pushcfunction(L, lua_param_get);
    lua_setfield(L, -2, "get");

    lua_pushcfunction(L, lua_param_set);
    lua_setfield(L, -2, "set");

    lua_pushcfunction(L, lua_param_reset);
    lua_setfield(L, -2, "reset");

    lua_pushcfunction(L, lua_param_undefine);
    lua_setfield(L, -2, "undefine");

    /* OSC binding */
    lua_pushcfunction(L, lua_param_bind_osc);
    lua_setfield(L, -2, "bind_osc");

    lua_pushcfunction(L, lua_param_unbind_osc);
    lua_setfield(L, -2, "unbind_osc");

    /* MIDI binding */
    lua_pushcfunction(L, lua_param_bind_midi);
    lua_setfield(L, -2, "bind_midi");

    lua_pushcfunction(L, lua_param_unbind_midi);
    lua_setfield(L, -2, "unbind_midi");

    /* Listing */
    lua_pushcfunction(L, lua_param_list);
    lua_setfield(L, -2, "list");

    lua_pushcfunction(L, lua_param_count);
    lua_setfield(L, -2, "count");

    lua_pushcfunction(L, lua_param_reset_all);
    lua_setfield(L, -2, "reset_all");

    /* MIDI input for CC */
    lua_pushcfunction(L, lua_param_midi_in_open);
    lua_setfield(L, -2, "midi_in_open");

    lua_pushcfunction(L, lua_param_midi_in_close);
    lua_setfield(L, -2, "midi_in_close");

    lua_pushcfunction(L, lua_param_midi_in_list);
    lua_setfield(L, -2, "midi_in_list");

    lua_setfield(L, -2, "param");  /* Set as loki.param */
}

/* ======================= Plugin (Minihost) Lua Bindings ======================= */

/* Lua API: loki.plugin.load(slot, path) - Load a VST3/AU plugin */
static int lua_plugin_load(lua_State *L) {
    int slot = (int)luaL_checkinteger(L, 1);
    const char* path = luaL_checkstring(L, 2);
    int result = shared_minihost_load(slot, path);
    lua_pushboolean(L, result == 0);
    return 1;
}

/* Lua API: loki.plugin.unload(slot) - Unload a plugin */
static int lua_plugin_unload(lua_State *L) {
    int slot = (int)luaL_checkinteger(L, 1);
    shared_minihost_unload(slot);
    return 0;
}

/* Lua API: loki.plugin.has_plugin(slot) - Check if a plugin is loaded */
static int lua_plugin_has_plugin(lua_State *L) {
    int slot = (int)luaL_checkinteger(L, 1);
    lua_pushboolean(L, shared_minihost_has_plugin(slot));
    return 1;
}

/* Lua API: loki.plugin.name(slot) - Get the plugin name */
static int lua_plugin_name(lua_State *L) {
    int slot = (int)luaL_checkinteger(L, 1);
    const char* name = shared_minihost_get_plugin_name(slot);
    if (name) {
        lua_pushstring(L, name);
    } else {
        lua_pushnil(L);
    }
    return 1;
}

/* Lua API: loki.plugin.enable() - Enable the minihost backend */
static int lua_plugin_enable(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (ctx && ctx->model.shared) {
        ctx->model.shared->minihost_enabled = 1;
        int result = shared_minihost_enable();
        lua_pushboolean(L, result == 0);
    } else {
        lua_pushboolean(L, 0);
    }
    return 1;
}

/* Lua API: loki.plugin.disable() - Disable the minihost backend */
static int lua_plugin_disable(lua_State *L) {
    editor_ctx_t *ctx = loki_lua_get_editor_context(L);
    if (ctx && ctx->model.shared) {
        ctx->model.shared->minihost_enabled = 0;
        shared_minihost_disable();
    }
    return 0;
}

/* Lua API: loki.plugin.is_enabled() - Check if minihost is enabled */
static int lua_plugin_is_enabled(lua_State *L) {
    lua_pushboolean(L, shared_minihost_is_enabled());
    return 1;
}

/* Lua API: loki.plugin.is_available() - Check if minihost is compiled in */
static int lua_plugin_is_available(lua_State *L) {
    lua_pushboolean(L, shared_minihost_is_available());
    return 1;
}

/* Lua API: loki.plugin.set_param(slot, index, value) - Set a parameter (0-1) */
static int lua_plugin_set_param(lua_State *L) {
    int slot = (int)luaL_checkinteger(L, 1);
    int index = (int)luaL_checkinteger(L, 2);
    float value = (float)luaL_checknumber(L, 3);
    int result = shared_minihost_set_param(slot, index, value);
    lua_pushboolean(L, result == 0);
    return 1;
}

/* Lua API: loki.plugin.get_param(slot, index) - Get a parameter value */
static int lua_plugin_get_param(lua_State *L) {
    int slot = (int)luaL_checkinteger(L, 1);
    int index = (int)luaL_checkinteger(L, 2);
    lua_pushnumber(L, shared_minihost_get_param(slot, index));
    return 1;
}

/* Lua API: loki.plugin.num_params(slot) - Get number of parameters */
static int lua_plugin_num_params(lua_State *L) {
    int slot = (int)luaL_checkinteger(L, 1);
    lua_pushinteger(L, shared_minihost_get_num_params(slot));
    return 1;
}

/* Lua API: loki.plugin.params(slot) - List all parameter names */
static int lua_plugin_params(lua_State *L) {
    int slot = (int)luaL_checkinteger(L, 1);
    int count = shared_minihost_get_num_params(slot);

    lua_newtable(L);
    for (int i = 0; i < count; i++) {
        char name[256];
        if (shared_minihost_get_param_name(slot, i, name, sizeof(name)) == 0) {
            lua_pushstring(L, name);
        } else {
            lua_pushfstring(L, "param_%d", i);
        }
        lua_rawseti(L, -2, i + 1);  /* 1-based index */
    }
    return 1;
}

/* Lua API: loki.plugin.save_state(slot, path) - Save plugin state */
static int lua_plugin_save_state(lua_State *L) {
    int slot = (int)luaL_checkinteger(L, 1);
    const char* path = luaL_checkstring(L, 2);
    int result = shared_minihost_save_state(slot, path);
    lua_pushboolean(L, result == 0);
    return 1;
}

/* Lua API: loki.plugin.load_state(slot, path) - Load plugin state */
static int lua_plugin_load_state(lua_State *L) {
    int slot = (int)luaL_checkinteger(L, 1);
    const char* path = luaL_checkstring(L, 2);
    int result = shared_minihost_load_state(slot, path);
    lua_pushboolean(L, result == 0);
    return 1;
}

/* Callback data for plugin scanning */
typedef struct {
    lua_State *L;
    int table_index;
} scan_callback_data;

static void lua_plugin_scan_callback(const char* name, const char* path, void* userdata) {
    scan_callback_data* data = (scan_callback_data*)userdata;
    lua_State *L = data->L;

    /* Create entry table {name=..., path=...} */
    lua_newtable(L);
    lua_pushstring(L, name);
    lua_setfield(L, -2, "name");
    lua_pushstring(L, path);
    lua_setfield(L, -2, "path");

    /* Append to result table */
    lua_rawseti(L, data->table_index, lua_rawlen(L, data->table_index) + 1);
}

/* Lua API: loki.plugin.scan(directory) - Scan for plugins */
static int lua_plugin_scan(lua_State *L) {
    const char* dir = luaL_checkstring(L, 1);

    lua_newtable(L);  /* Result table */
    int table_index = lua_gettop(L);

    scan_callback_data data = { L, table_index };
    shared_minihost_scan_directory(dir, lua_plugin_scan_callback, &data);

    return 1;
}

/* Register plugin module as loki.plugin subtable */
static void lua_register_plugin_module(lua_State *L) {
    /* Assumes loki table is on top of stack */
    lua_newtable(L);  /* Create plugin subtable */

    /* Plugin loading */
    lua_pushcfunction(L, lua_plugin_load);
    lua_setfield(L, -2, "load");

    lua_pushcfunction(L, lua_plugin_unload);
    lua_setfield(L, -2, "unload");

    lua_pushcfunction(L, lua_plugin_has_plugin);
    lua_setfield(L, -2, "has_plugin");

    lua_pushcfunction(L, lua_plugin_name);
    lua_setfield(L, -2, "name");

    /* Enable/disable */
    lua_pushcfunction(L, lua_plugin_enable);
    lua_setfield(L, -2, "enable");

    lua_pushcfunction(L, lua_plugin_disable);
    lua_setfield(L, -2, "disable");

    lua_pushcfunction(L, lua_plugin_is_enabled);
    lua_setfield(L, -2, "is_enabled");

    lua_pushcfunction(L, lua_plugin_is_available);
    lua_setfield(L, -2, "is_available");

    /* Parameters */
    lua_pushcfunction(L, lua_plugin_set_param);
    lua_setfield(L, -2, "set_param");

    lua_pushcfunction(L, lua_plugin_get_param);
    lua_setfield(L, -2, "get_param");

    lua_pushcfunction(L, lua_plugin_num_params);
    lua_setfield(L, -2, "num_params");

    lua_pushcfunction(L, lua_plugin_params);
    lua_setfield(L, -2, "params");

    /* State persistence */
    lua_pushcfunction(L, lua_plugin_save_state);
    lua_setfield(L, -2, "save_state");

    lua_pushcfunction(L, lua_plugin_load_state);
    lua_setfield(L, -2, "load_state");

    /* Scanning */
    lua_pushcfunction(L, lua_plugin_scan);
    lua_setfield(L, -2, "scan");

    lua_setfield(L, -2, "plugin");  /* Set as loki.plugin */
}

/* Register midi module as loki.midi subtable */
static void lua_register_midi_module(lua_State *L) {
    /* Assumes loki table is on top of stack */
    lua_newtable(L);  /* Create midi subtable */

    /* Port enumeration */
    lua_pushcfunction(L, lua_midi_port_count);
    lua_setfield(L, -2, "port_count");

    lua_pushcfunction(L, lua_midi_port_name);
    lua_setfield(L, -2, "port_name");

    lua_pushcfunction(L, lua_midi_list_ports);
    lua_setfield(L, -2, "list_ports");

    /* Port connection */
    lua_pushcfunction(L, lua_midi_open_port);
    lua_setfield(L, -2, "open_port");

    lua_pushcfunction(L, lua_midi_open_by_name);
    lua_setfield(L, -2, "open_by_name");

    lua_pushcfunction(L, lua_midi_open_virtual);
    lua_setfield(L, -2, "open_virtual");

    lua_pushcfunction(L, lua_midi_close);
    lua_setfield(L, -2, "close");

    lua_pushcfunction(L, lua_midi_is_open);
    lua_setfield(L, -2, "is_open");

    /* Export */
    lua_pushcfunction(L, lua_midi_export);
    lua_setfield(L, -2, "export");

    lua_setfield(L, -2, "midi");  /* Set as loki.midi */
}

void loki_lua_bind_minimal(lua_State *L) {
    lua_getglobal(L, "loki");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
    }

    lua_pushcfunction(L, lua_loki_status_stdout);
    lua_setfield(L, -2, "status");

    lua_pushcfunction(L, lua_loki_register_language);
    lua_setfield(L, -2, "register_language");

    lua_newtable(L); /* storage for registered help */
    lua_setfield(L, -2, "__repl_help");

    lua_newtable(L); /* repl helpers */
    lua_pushcfunction(L, lua_loki_repl_register);
    lua_setfield(L, -2, "register");
    lua_setfield(L, -2, "repl");

    lua_setglobal(L, "loki");
}

/* Initialize Lua API */
void loki_lua_bind_editor(lua_State *L) {
    /* Create loki table */
    lua_newtable(L);

    /* Register functions */
    lua_pushcfunction(L, lua_loki_status);
    lua_setfield(L, -2, "status");

    lua_pushcfunction(L, lua_loki_get_line);
    lua_setfield(L, -2, "get_line");

    lua_pushcfunction(L, lua_loki_get_lines);
    lua_setfield(L, -2, "get_lines");

    lua_pushcfunction(L, lua_loki_get_cursor);
    lua_setfield(L, -2, "get_cursor");

    lua_pushcfunction(L, lua_loki_insert_text);
    lua_setfield(L, -2, "insert_text");

    lua_pushcfunction(L, lua_loki_stream_text);
    lua_setfield(L, -2, "stream_text");

    lua_pushcfunction(L, lua_loki_get_filename);
    lua_setfield(L, -2, "get_filename");

    lua_pushcfunction(L, lua_loki_set_color);
    lua_setfield(L, -2, "set_color");

    lua_pushcfunction(L, lua_loki_set_theme);
    lua_setfield(L, -2, "set_theme");

    /* Display settings */
    lua_pushcfunction(L, lua_loki_line_numbers);
    lua_setfield(L, -2, "line_numbers");

    /* Modal system functions */
    lua_pushcfunction(L, lua_loki_get_mode);
    lua_setfield(L, -2, "get_mode");

    lua_pushcfunction(L, lua_loki_set_mode);
    lua_setfield(L, -2, "set_mode");

    lua_pushcfunction(L, lua_loki_register_command);
    lua_setfield(L, -2, "register_command");

    lua_pushcfunction(L, lua_loki_register_ex_command);
    lua_setfield(L, -2, "register_ex_command");

    lua_pushcfunction(L, lua_loki_register_language);
    lua_setfield(L, -2, "register_language");

    /* Keybinding functions */
    lua_pushcfunction(L, lua_loki_keymap);
    lua_setfield(L, -2, "keymap");

    lua_pushcfunction(L, lua_loki_keyunmap);
    lua_setfield(L, -2, "keyunmap");

    lua_newtable(L); /* storage for registered help */
    lua_setfield(L, -2, "__repl_help");

    lua_newtable(L); /* repl helpers */
    lua_pushcfunction(L, lua_loki_repl_register);
    lua_setfield(L, -2, "register");
    lua_setfield(L, -2, "repl");

    /* Highlight constants */
    lua_newtable(L);
    lua_pushinteger(L, HL_NORMAL);
    lua_setfield(L, -2, "normal");
    lua_pushinteger(L, HL_NONPRINT);
    lua_setfield(L, -2, "nonprint");
    lua_pushinteger(L, HL_COMMENT);
    lua_setfield(L, -2, "comment");
    lua_pushinteger(L, HL_MLCOMMENT);
    lua_setfield(L, -2, "mlcomment");
    lua_pushinteger(L, HL_KEYWORD1);
    lua_setfield(L, -2, "keyword1");
    lua_pushinteger(L, HL_KEYWORD2);
    lua_setfield(L, -2, "keyword2");
    lua_pushinteger(L, HL_STRING);
    lua_setfield(L, -2, "string");
    lua_pushinteger(L, HL_NUMBER);
    lua_setfield(L, -2, "number");
    lua_pushinteger(L, HL_MATCH);
    lua_setfield(L, -2, "match");
    lua_setfield(L, -2, "hl");

    /* Register link module as loki.link */
    lua_register_link_module(L);

    /* Register midi module as loki.midi */
    lua_register_midi_module(L);

    /* Register osc module as loki.osc */
    lua_register_osc_module(L);

    /* Register param module as loki.param */
    lua_register_param_module(L);

    /* Register plugin module as loki.plugin */
    lua_register_plugin_module(L);

#ifdef BUILD_PLUGIN_SQLITE
    /* Register fts module as loki.fts */
    lua_register_fts_module(L);
#endif

    /* Set as global 'loki' */
    lua_setglobal(L, "loki");

    /* Create global aliases for commonly used submodules */
    /* Allows: midi.list_ports() instead of loki.midi.list_ports() */
    lua_getglobal(L, "loki");
    lua_getfield(L, -1, "midi");
    lua_setglobal(L, "midi");
    lua_getfield(L, -1, "link");
    lua_setglobal(L, "link");
    lua_getfield(L, -1, "osc");
    lua_setglobal(L, "osc");
    lua_getfield(L, -1, "param");
    lua_setglobal(L, "param");
    lua_getfield(L, -1, "plugin");
    lua_setglobal(L, "plugin");
#ifdef BUILD_PLUGIN_SQLITE
    lua_getfield(L, -1, "fts");
    lua_setglobal(L, "fts");
#endif
    lua_pop(L, 1);  /* pop loki table */

    /* Register language-specific Lua APIs via the bridge */
    loki_lang_register_lua_apis(L);
}

static void loki_lua_report(const struct loki_lua_opts *opts, const char *fmt, ...) {
    if (!fmt) return;

    char message[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(message, sizeof(message), fmt, ap);
    va_end(ap);

    if (opts && opts->reporter) {
        opts->reporter(message, opts->reporter_userdata);
    } else {
        fprintf(stderr, "%s\n", message);
    }
}

/* Load init.lua: try override, .psnd/init.lua (local), then ~/.psnd/init.lua */
int loki_lua_load_config(lua_State *L, const struct loki_lua_opts *opts) {
    if (!L) return -1;

    const char *override = (opts && opts->config_override && opts->config_override[0] != '\0')
                               ? opts->config_override
                               : NULL;
    const char *project_root = NULL;
    char init_path[1024];
    int loaded = 0;

    if (opts && opts->project_root && opts->project_root[0] != '\0') {
        project_root = opts->project_root;
    }

    if (override && override[0] != '\0') {
        if (luaL_dofile(L, override) != LUA_OK) {
            const char *err = lua_tostring(L, -1);
            loki_lua_report(opts, "Lua init error (%s): %s", override, err ? err : "unknown");
            lua_pop(L, 1);
            return -1;
        }
        return 0;
    }

    /* Try local config dir init.lua first (project-specific) */
    if (project_root) {
        snprintf(init_path, sizeof(init_path), "%s/" PSND_CONFIG_DIR "/init.lua", project_root);
    } else {
        snprintf(init_path, sizeof(init_path), PSND_CONFIG_DIR "/init.lua");
    }

    if (access(init_path, R_OK) == 0) {
        if (luaL_dofile(L, init_path) != LUA_OK) {
            const char *err = lua_tostring(L, -1);
            loki_lua_report(opts, "Lua init error (%s): %s", init_path, err ? err : "unknown");
            lua_pop(L, 1);
            return -1;
        }
        loaded = 1;
    }

    if (!loaded) {
        const char *home = getenv("HOME");
        if (home && home[0] != '\0') {
            snprintf(init_path, sizeof(init_path), "%s/" PSND_CONFIG_DIR "/init.lua", home);
            if (access(init_path, R_OK) == 0) {
                if (luaL_dofile(L, init_path) != LUA_OK) {
                    const char *err = lua_tostring(L, -1);
                    loki_lua_report(opts, "Lua init error (%s): %s", init_path, err ? err : "unknown");
                    lua_pop(L, 1);
                    return -1;
                }
                loaded = 1;
            }
        }
    }

    return loaded ? 0 : 1;
}

static void loki_lua_extend_path(lua_State *L, const struct loki_lua_opts *opts) {
    if (!L) return;

    char addition[4096];
    addition[0] = '\0';
    size_t used = 0;
    size_t remaining = sizeof(addition);

    const char *project_root = (opts && opts->project_root && opts->project_root[0] != '\0')
                                   ? opts->project_root
                                   : ".";
    int wrote = snprintf(addition + used, remaining,
                         "%s/" PSND_CONFIG_DIR "/?.lua;%s/" PSND_CONFIG_DIR "/?/init.lua;",
                         project_root, project_root);
    if (wrote > 0 && (size_t)wrote < remaining) {
        used += (size_t)wrote;
        remaining -= (size_t)wrote;
    } else {
        remaining = remaining > 0 ? 1 : 0;
        used = sizeof(addition) - remaining;
    }

    const char *home = getenv("HOME");
    if (home && home[0] != '\0' && remaining > 1) {
        wrote = snprintf(addition + used, remaining,
                         "%s/" PSND_CONFIG_DIR "/?.lua;%s/" PSND_CONFIG_DIR "/?/init.lua;",
                         home, home);
        if (wrote > 0 && (size_t)wrote < remaining) {
            used += (size_t)wrote;
            remaining -= (size_t)wrote;
        } else {
            remaining = remaining > 0 ? 1 : 0;
            used = sizeof(addition) - remaining;
        }
    }

    const char *extra = NULL;
    if (opts && opts->extra_lua_path && opts->extra_lua_path[0] != '\0') {
        extra = opts->extra_lua_path;
    } else {
        const char *env_extra = getenv("LOKI_LUA_PATH");
        if (env_extra && env_extra[0] != '\0') {
            extra = env_extra;
        }
    }
    if (extra && remaining > 1) {
        wrote = snprintf(addition + used, remaining, "%s;", extra);
        if (wrote > 0 && (size_t)wrote < remaining) {
            used += (size_t)wrote;
            remaining -= (size_t)wrote;
        }
    }

    if (used == 0) {
        return;
    }

    if (addition[used - 1] == ';') {
        addition[used - 1] = '\0';
    }

    lua_getglobal(L, "package");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    lua_getfield(L, -1, "path");
    const char *current = lua_tostring(L, -1);
    if (!current) current = "";

    lua_pushfstring(L, "%s;%s", current, addition);
    lua_setfield(L, -3, "path");

    lua_pop(L, 2); /* pop path and package */
}

void loki_lua_install_namespaces(lua_State *L) {
    if (!L) return;

    lua_getglobal(L, "loki");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    static const char *const shim =
        "local loki = ...\n"
        "local function ensure(tbl, key)\n"
        "  local value = rawget(tbl, key)\n"
        "  if type(value) ~= 'table' then\n"
        "    value = {}\n"
        "    rawset(tbl, key, value)\n"
        "  end\n"
        "  return value\n"
        "end\n"
        "local editor = ensure(loki, 'editor')\n"
        "editor.buffer = ensure(editor, 'buffer')\n"
        "editor.status = ensure(editor, 'status')\n"
        "function editor.status.set(message)\n"
        "  return loki.status(message)\n"
        "end\n"
        "function editor.buffer.get_line(idx)\n"
        "  return loki.get_line(idx)\n"
        "end\n"
        "function editor.buffer.line_count()\n"
        "  return loki.get_lines()\n"
        "end\n"
        "function editor.buffer.insert(text)\n"
        "  return loki.insert_text(text)\n"
        "end\n";

    if (luaL_loadstring(L, shim) == LUA_OK) {
        lua_pushvalue(L, -2); /* push loki table */
        if (lua_pcall(L, 1, 0, 0) != LUA_OK) {
            const char *err = lua_tostring(L, -1);
            fprintf(stderr, "Failed to install editor namespace: %s\n", err ? err : "unknown error");
            lua_pop(L, 1);
        }
    } else {
        const char *err = lua_tostring(L, -1);
        fprintf(stderr, "Failed to compile editor namespace shim: %s\n", err ? err : "unknown error");
        lua_pop(L, 1);
    }

    lua_pop(L, 1); /* loki */
}

const char *loki_lua_runtime(void) {
#if defined(LUAJIT_VERSION)
    return "LuaJIT " LUAJIT_VERSION;
#elif defined(LUA_VERSION_MAJOR) && defined(LUA_VERSION_MINOR)
    return "Lua " LUA_VERSION_MAJOR "." LUA_VERSION_MINOR;
#elif defined(LUA_VERSION)
    return LUA_VERSION;
#else
    return "Lua";
#endif
}

lua_State *loki_lua_bootstrap(editor_ctx_t *ctx, const struct loki_lua_opts *opts) {
    struct loki_lua_opts effective = {
        .bind_editor = 1,
        .bind_http = 0,  /* HTTP support removed */
        .load_config = 1,
        .config_override = NULL,
        .project_root = NULL,
        .extra_lua_path = NULL,
        .reporter = NULL,
        .reporter_userdata = NULL,
    };

    if (opts) {
        effective.bind_editor = opts->bind_editor ? 1 : 0;
        effective.bind_http = 0;  /* HTTP support removed */
        effective.load_config = opts->load_config ? 1 : 0;
        effective.config_override = opts->config_override;
        effective.project_root = opts->project_root;
        effective.extra_lua_path = opts->extra_lua_path;
        effective.reporter = opts->reporter;
        effective.reporter_userdata = opts->reporter_userdata;
    }

    lua_State *L = luaL_newstate();
    if (!L) {
        loki_lua_report(&effective, "Failed to allocate Lua state");
        return NULL;
    }

    /* Store editor context in Lua registry for retrieval by API functions */
    if (ctx) {
        lua_pushlightuserdata(L, (void *)&editor_ctx_registry_key);
        lua_pushlightuserdata(L, ctx);
        lua_settable(L, LUA_REGISTRYINDEX);
    }

#ifdef LUA_SANDBOX
    /* Sandboxed mode: load only safe libraries */

    /* Base library (print, pairs, ipairs, type, tonumber, tostring, etc.) */
    luaL_requiref(L, "_G", luaopen_base, 1);
    lua_pop(L, 1);

    /* Remove dangerous functions from base */
    lua_pushnil(L);
    lua_setglobal(L, "load");       /* load(chunk) - execute arbitrary code */
    lua_pushnil(L);
    lua_setglobal(L, "loadfile");   /* loadfile(path) - execute arbitrary file */
    lua_pushnil(L);
    lua_setglobal(L, "dofile");     /* dofile(path) - execute arbitrary file */

    /* Safe libraries */
    luaL_requiref(L, LUA_TABLIBNAME, luaopen_table, 1);
    lua_pop(L, 1);
    luaL_requiref(L, LUA_STRLIBNAME, luaopen_string, 1);
    lua_pop(L, 1);
    luaL_requiref(L, LUA_MATHLIBNAME, luaopen_math, 1);
    lua_pop(L, 1);
    luaL_requiref(L, LUA_UTF8LIBNAME, luaopen_utf8, 1);
    lua_pop(L, 1);
    luaL_requiref(L, LUA_COLIBNAME, luaopen_coroutine, 1);
    lua_pop(L, 1);

    /* Package library for require() - needed for .psnd modules */
    luaL_requiref(L, LUA_LOADLIBNAME, luaopen_package, 1);
    lua_pop(L, 1);

    /* No native code: package.loadlib and the C searchers load shared
     * libraries, which would bypass every restriction above. */
    lua_getglobal(L, "package");
    lua_pushnil(L);
    lua_setfield(L, -2, "loadlib");
    lua_pushliteral(L, "");
    lua_setfield(L, -2, "cpath");
    lua_getfield(L, -1, "searchers");   /* preload, Lua, C, all-in-one */
    lua_pushnil(L);
    lua_rawseti(L, -2, 4);
    lua_pushnil(L);
    lua_rawseti(L, -2, 3);
    lua_pop(L, 2);

    /* Skipped (dangerous):
     * - os: os.execute(), os.remove(), os.rename(), os.exit()
     * - io: io.open(), io.popen(), file read/write
     * - debug: can bypass metatables, inspect/modify internals
     */
#else
    /* Unsandboxed mode: full Lua access (use -DLUA_SANDBOX=OFF) */
    luaL_openlibs(L);
#endif
    loki_lua_extend_path(L, &effective);

    if (effective.bind_editor) {
        loki_lua_bind_editor(L);
    } else {
        loki_lua_bind_minimal(L);
    }

    if (effective.load_config) {
        if (loki_lua_load_config(L, &effective) < 0) {
            /* Leave state usable even if config fails; errors already reported */
        }
    }

    loki_lua_install_namespaces(L);

    return L;
}
/* =========================== Lua REPL Functions ========================== */
/* Extracted from loki_core.c - handles the embedded Lua REPL interface */

/* Forward declarations */
static int lua_repl_handle_builtin(editor_ctx_t *ctx, const char *cmd, size_t len);

void lua_repl_render(editor_ctx_t *ctx, struct abuf *ab) {
    t_lua_repl *repl = ctx_repl(ctx);
    if (!ctx || !repl || !repl->active) return;

    terminal_buffer_append(ab,"\r\n",2);

    int start = repl->log_len - LUA_REPL_OUTPUT_ROWS;
    if (start < 0) start = 0;
    int rendered = 0;

    for (int i = start; i < repl->log_len; i++) {
        const char *line = repl->log[i] ? repl->log[i] : "";
        int take = (int)strlen(line);
        if (take > ctx->view.screencols) take = ctx->view.screencols;
        terminal_buffer_append(ab,"\x1b[0K",4);
        if (take > 0) terminal_buffer_append(ab,line,take);
        terminal_buffer_append(ab,"\r\n",2);
        rendered++;
    }

    while (rendered < LUA_REPL_OUTPUT_ROWS) {
        terminal_buffer_append(ab,"\x1b[0K\r\n",6);
        rendered++;
    }

    terminal_buffer_append(ab,"\x1b[0K",4);
    terminal_buffer_append(ab,LUA_REPL_PROMPT,strlen(LUA_REPL_PROMPT));

    int prompt_len = (int)strlen(LUA_REPL_PROMPT);
    int available = ctx->view.screencols - prompt_len;
    if (available < 0) available = 0;
    if (available > 0 && repl->input_len > 0) {
        int shown = repl->input_len;
        if (shown > available) shown = available;
        terminal_buffer_append(ab,repl->input,shown);
    }
}

/* =========================== Lua REPL Helpers ============================ */

static void lua_repl_clear_input(t_lua_repl *repl) {
    repl->input_len = 0;
    repl->input[0] = '\0';
}

static void lua_repl_append_log_owned(editor_ctx_t *ctx, char *line) {
    t_lua_repl *repl = ctx_repl(ctx);
    if (!ctx || !repl || !line) return;
    if (repl->log_len == LUA_REPL_LOG_MAX) {
        free(repl->log[0]);
        memmove(repl->log, repl->log + 1,
                sizeof(char*) * (LUA_REPL_LOG_MAX - 1));
        repl->log_len--;
    }
    repl->log[repl->log_len++] = line;
}

void lua_repl_append_log(editor_ctx_t *ctx, const char *line) {
    if (!ctx || !line) return;
    char *copy = strdup(line);
    if (!copy) {
        editor_set_status_msg(ctx, "Lua REPL: out of memory");
        return;
    }
    lua_repl_append_log_owned(ctx, copy);
}

static void lua_repl_log_prefixed(editor_ctx_t *ctx, const char *prefix, const char *text) {
    if (!ctx || !prefix) prefix = "";
    if (!text) text = "";

    size_t prefix_len = strlen(prefix);
    const char *line = text;
    do {
        const char *newline = strchr(line, '\n');
        size_t segment_len = newline ? (size_t)(newline - line) : strlen(line);
        size_t total = prefix_len + segment_len;
        char *entry = malloc(total + 1);
        if (!entry) {
            editor_set_status_msg(ctx, "Lua REPL: out of memory");
            return;
        }
        memcpy(entry, prefix, prefix_len);
        if (segment_len) memcpy(entry + prefix_len, line, segment_len);
        entry[total] = '\0';
        lua_repl_append_log_owned(ctx, entry);
        if (!newline) break;
        line = newline + 1;
        if (*line == '\0') {
            /* Preserve empty trailing line */
            char *blank = strdup(prefix);
            if (!blank) {
                editor_set_status_msg(ctx, "Lua REPL: out of memory");
                return;
            }
            lua_repl_append_log_owned(ctx, blank);
            break;
        }
    } while (1);
}

static void lua_repl_reset_log(t_lua_repl *repl) {
    for (int i = 0; i < repl->log_len; i++) {
        free(repl->log[i]);
        repl->log[i] = NULL;
    }
    repl->log_len = 0;
}

static const char *lua_repl_top_to_string(lua_State *L, size_t *len) {
#if LUA_VERSION_NUM >= 502
    return luaL_tolstring(L, -1, len);
#else
    if (luaL_callmeta(L, -1, "__tostring")) {
        if (!lua_isstring(L, -1)) {
            lua_pop(L, 1);
            return NULL;
        }
        return lua_tolstring(L, -1, len);
    }
    return lua_tolstring(L, -1, len);
#endif
}

static void lua_repl_push_history(editor_ctx_t *ctx, const char *cmd) {
    t_lua_repl *repl = ctx_repl(ctx);
    if (!ctx || !repl || !cmd || !*cmd) return;
    size_t len = strlen(cmd);
    int all_space = 1;
    for (size_t i = 0; i < len; i++) {
        if (!isspace((unsigned char)cmd[i])) {
            all_space = 0;
            break;
        }
    }
    if (all_space) return;

    if (repl->history_len == LUA_REPL_HISTORY_MAX) {
        free(repl->history[0]);
        memmove(repl->history, repl->history + 1,
                sizeof(char*) * (LUA_REPL_HISTORY_MAX - 1));
        repl->history_len--;
    }

    if (repl->history_len > 0) {
        const char *last = repl->history[repl->history_len - 1];
        if (last && strcmp(last, cmd) == 0) {
            repl->history_index = -1;
            return;
        }
    }

    char *copy = strdup(cmd);
    if (!copy) {
        editor_set_status_msg(ctx, "Lua REPL: out of memory");
        return;
    }
    repl->history[repl->history_len++] = copy;
    repl->history_index = -1;
}

static void lua_repl_history_apply(editor_ctx_t *ctx, t_lua_repl *repl) {
    if (repl->history_index < 0 || repl->history_index >= repl->history_len)
        return;
    const char *src = repl->history[repl->history_index];
    if (!src) {
        lua_repl_clear_input(repl);
        return;
    }
    size_t copy_len = strlen(src);
    if (copy_len > KILO_QUERY_LEN) copy_len = KILO_QUERY_LEN;
    int prompt_len = (int)strlen(LUA_REPL_PROMPT);
    if (ctx && ctx->view.screencols > prompt_len) {
        int max_cols = ctx->view.screencols - prompt_len;
        if ((int)copy_len > max_cols) copy_len = max_cols;
    }
    memcpy(repl->input, src, copy_len);
    repl->input[copy_len] = '\0';
    repl->input_len = (int)copy_len;
}

static int lua_repl_input_has_content(const t_lua_repl *repl) {
    for (int i = 0; i < repl->input_len; i++) {
        if (!isspace((unsigned char)repl->input[i])) return 1;
    }
    return 0;
}

static void lua_repl_emit_registered_help(editor_ctx_t *ctx) {
    lua_State *L = ctx_L(ctx);
    if (!ctx || !L) return;

    lua_getglobal(L, "loki");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    lua_getfield(L, -1, "__repl_help");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 2);
        return;
    }

    int len = (int)lua_rawlen(L, -1);
    if (len == 0) {
        lua_pop(L, 2);
        return;
    }

    lua_repl_log_prefixed(ctx, "= ", "Project commands:");

    lua_pushnil(L);
    while (lua_next(L, -2) != 0) {
        const char *name = NULL;
        const char *desc = NULL;
        const char *example = NULL;

        lua_getfield(L, -1, "name");
        if (lua_isstring(L, -1)) name = lua_tostring(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "description");
        if (lua_isstring(L, -1)) desc = lua_tostring(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "example");
        if (lua_isstring(L, -1)) example = lua_tostring(L, -1);
        lua_pop(L, 1);

        if (name && desc) {
            char buf[256];
            snprintf(buf, sizeof(buf), "  %s - %s", name, desc);
            lua_repl_append_log(ctx, buf);
        }
        if (example) {
            char buf[256];
            snprintf(buf, sizeof(buf), "    e.g. %s", example);
            lua_repl_append_log(ctx, buf);
        }

        lua_pop(L, 1); /* pop value, keep key for next iteration */
    }

    lua_pop(L, 2); /* __repl_help, loki */
}

static void lua_repl_execute_current(editor_ctx_t *ctx) {
    lua_State *L = ctx_L(ctx);
    t_lua_repl *repl = ctx_repl(ctx);
    if (!ctx || !L || !repl) {
        if (ctx) lua_repl_append_log(ctx, "! Lua interpreter not available");
        return;
    }

    if (!lua_repl_input_has_content(repl)) {
        lua_repl_clear_input(repl);
        return;
    }

    lua_repl_log_prefixed(ctx, LUA_REPL_PROMPT, repl->input);
    lua_repl_push_history(ctx, repl->input);

    const char *trim = repl->input;
    while (*trim && isspace((unsigned char)*trim)) trim++;
    size_t tlen = strlen(trim);
    while (tlen > 0 && isspace((unsigned char)trim[tlen-1])) tlen--;

    if (lua_repl_handle_builtin(ctx, trim, tlen)) {
        lua_repl_clear_input(repl);
        return;
    }

    int base = lua_gettop(L);
    if (luaL_loadbuffer(L, repl->input, (size_t)repl->input_len,
                        "repl") != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        lua_repl_log_prefixed(ctx, "! ", err ? err : "(unknown error)");
        lua_pop(L, 1);
        lua_settop(L, base);
        lua_repl_clear_input(repl);
        return;
    }

    int status = lua_pcall(L, 0, LUA_MULTRET, 0);
    if (status != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        lua_repl_log_prefixed(ctx, "! ", err ? err : "(unknown error)");
        lua_pop(L, 1);
        lua_settop(L, base);
        lua_repl_clear_input(repl);
        return;
    }

    int results = lua_gettop(L) - base;
    if (results == 0) {
        lua_repl_log_prefixed(ctx, "= ", "ok");
    } else {
        for (int i = 0; i < results; i++) {
            lua_pushvalue(L, base + 1 + i);
            size_t len = 0;
            const char *res = lua_repl_top_to_string(L, &len);
            if (res) {
                lua_repl_log_prefixed(ctx, "= ", res);
            } else {
                lua_repl_log_prefixed(ctx, "= ", "(non-printable)");
            }
            lua_settop(L, base + results);
        }
    }
    lua_settop(L, base);
    lua_repl_clear_input(repl);
}

static int lua_repl_iequals(const char *cmd, size_t len, const char *word) {
    size_t wlen = strlen(word);
    if (len != wlen) return 0;
    for (size_t i = 0; i < len; i++) {
        if (tolower((unsigned char)cmd[i]) != tolower((unsigned char)word[i]))
            return 0;
    }
    return 1;
}

static int lua_repl_handle_builtin(editor_ctx_t *ctx, const char *cmd, size_t len) {
    if (!ctx || !cmd) return 0;
    while (len && isspace((unsigned char)*cmd)) {
        cmd++;
        len--;
    }
    while (len && isspace((unsigned char)cmd[len-1])) len--;
    if (len == 0) return 0;

    if (cmd[0] == ':') {
        cmd++;
        if (len) len--;
        while (len && isspace((unsigned char)*cmd)) {
            cmd++;
            len--;
        }
        while (len && isspace((unsigned char)cmd[len-1])) len--;
        if (len == 0) return 0;
    }

    if ((len == 1 && cmd[0] == '?') || lua_repl_iequals(cmd, len, "help")) {
        lua_repl_log_prefixed(ctx, "= ", "Built-in commands:");
        lua_repl_append_log(ctx, "  help       Show this help message");
        lua_repl_append_log(ctx, "  history    Print recent commands");
        lua_repl_append_log(ctx, "  clear      Clear the REPL output log");
        lua_repl_append_log(ctx, "  clear-history  Drop saved input history");
        lua_repl_append_log(ctx, "  exit       Close the REPL panel");
        lua_repl_emit_registered_help(ctx);
        lua_repl_append_log(ctx, "  Lua code   Any other input runs inside loki's Lua state");
        return 1;
    }

    if (lua_repl_iequals(cmd, len, "clear")) {
        t_lua_repl *repl = ctx_repl(ctx);
        if (repl) lua_repl_reset_log(repl);
        lua_repl_log_prefixed(ctx, "= ", "Log cleared");
        return 1;
    }

    if (lua_repl_iequals(cmd, len, "history")) {
        t_lua_repl *repl = ctx_repl(ctx);
        if (!repl || repl->history_len == 0) {
            lua_repl_log_prefixed(ctx, "= ", "History is empty");
            return 1;
        }
        lua_repl_log_prefixed(ctx, "= ", "History (newest first):");
        int start = repl->history_len - 1;
        int shown = 0;
        for (int i = start; i >= 0; i--) {
            const char *entry = repl->history[i];
            if (!entry) continue;
            char buf[256];
            snprintf(buf, sizeof(buf), "  %d: %s", repl->history_len - i, entry);
            lua_repl_append_log(ctx, buf);
            shown++;
            if (shown >= 20) {
                lua_repl_append_log(ctx, "  ...");
                break;
            }
        }
        return 1;
    }

    if (lua_repl_iequals(cmd, len, "clear-history")) {
        t_lua_repl *repl = ctx_repl(ctx);
        if (repl) {
            for (int i = 0; i < repl->history_len; i++) {
                free(repl->history[i]);
                repl->history[i] = NULL;
            }
            repl->history_len = 0;
            repl->history_index = -1;
        }
        lua_repl_log_prefixed(ctx, "= ", "History cleared");
        return 1;
    }

    if (lua_repl_iequals(cmd, len, "exit") || lua_repl_iequals(cmd, len, "quit")) {
        t_lua_repl *repl = ctx_repl(ctx);
        if (repl) repl->active = 0;
        editor_update_repl_layout(ctx);
        editor_set_status_msg(ctx, "Lua REPL closed");
        return 1;
    }

    return 0;
}

/* Tab completion for Lua identifiers */
static void lua_repl_complete_input(editor_ctx_t *ctx) {
    if (!ctx || !ctx_L(ctx)) return;
    t_lua_repl *repl = ctx_repl(ctx);
    if (!repl) return;

    /* Nothing to complete if input is empty */
    if (repl->input_len == 0) return;

    /* Extract the word being completed (everything from last non-identifier char) */
    int word_start = repl->input_len;
    while (word_start > 0) {
        char c = repl->input[word_start - 1];
        if (!isalnum(c) && c != '_' && c != '.') break;
        word_start--;
    }

    /* No word to complete */
    if (word_start >= repl->input_len) return;

    char prefix[KILO_QUERY_LEN];
    int prefix_len = repl->input_len - word_start;
    memcpy(prefix, repl->input + word_start, prefix_len);
    prefix[prefix_len] = '\0';

    /* Handle table.field completion */
    char *dot = strchr(prefix, '.');
    char table_name[KILO_QUERY_LEN];
    char field_prefix[KILO_QUERY_LEN];

    if (dot) {
        /* Split into table and field parts */
        int table_len = dot - prefix;
        memcpy(table_name, prefix, table_len);
        table_name[table_len] = '\0';
        strncpy(field_prefix, dot + 1, KILO_QUERY_LEN - 1);
        field_prefix[KILO_QUERY_LEN - 1] = '\0';
    } else {
        table_name[0] = '\0';
        strncpy(field_prefix, prefix, KILO_QUERY_LEN - 1);
        field_prefix[KILO_QUERY_LEN - 1] = '\0';
    }

    /* Collect matches */
    char matches[100][KILO_QUERY_LEN];
    int match_count = 0;

    lua_State *L = ctx_L(ctx);

    if (table_name[0] != '\0') {
        /* Table.field completion - look in specific table */
        lua_getglobal(L, table_name);
        if (lua_istable(L, -1)) {
            lua_pushnil(L);
            while (lua_next(L, -2) != 0 && match_count < 100) {
                if (lua_type(L, -2) == LUA_TSTRING) {
                    const char *key = lua_tostring(L, -2);
                    if (strncmp(key, field_prefix, strlen(field_prefix)) == 0) {
                        snprintf(matches[match_count++], KILO_QUERY_LEN, "%s.%s", table_name, key);
                    }
                }
                lua_pop(L, 1);
            }
        }
        lua_pop(L, 1);
    } else {
        /* Global completion - look in _G */
        lua_pushglobaltable(L);
        lua_pushnil(L);
        while (lua_next(L, -2) != 0 && match_count < 100) {
            if (lua_type(L, -2) == LUA_TSTRING) {
                const char *key = lua_tostring(L, -2);
                if (strncmp(key, field_prefix, strlen(field_prefix)) == 0) {
                    strncpy(matches[match_count++], key, KILO_QUERY_LEN - 1);
                }
            }
            lua_pop(L, 1);
        }
        lua_pop(L, 1);
    }

    /* Handle matches */
    if (match_count == 0) {
        /* No matches - do nothing */
        return;
    } else if (match_count == 1) {
        /* Single match - complete it */
        const char *completion = matches[0];
        int completion_len = strlen(completion);
        int space_needed = word_start + completion_len;

        if (space_needed < KILO_QUERY_LEN) {
            /* Replace the partial word with the completion */
            memcpy(repl->input + word_start, completion, completion_len + 1);
            repl->input_len = space_needed;
        }
    } else {
        /* Multiple matches - find common prefix and show options */
        int common_len = strlen(matches[0]);
        for (int i = 1; i < match_count; i++) {
            int j = 0;
            while (j < common_len && matches[i][j] == matches[0][j]) {
                j++;
            }
            common_len = j;
        }

        /* Complete to common prefix */
        if (common_len > prefix_len) {
            int space_needed = word_start + common_len;
            if (space_needed < KILO_QUERY_LEN) {
                memcpy(repl->input + word_start, matches[0], common_len);
                repl->input_len = space_needed;
                repl->input[repl->input_len] = '\0';
            }
        }

        /* Show matches in status message */
        char status[256] = "";
        int status_len = 0;
        for (int i = 0; i < match_count && i < 5; i++) {
            if (i > 0) {
                status_len += snprintf(status + status_len, sizeof(status) - status_len, ", ");
            }
            status_len += snprintf(status + status_len, sizeof(status) - status_len, "%s", matches[i]);
        }
        if (match_count > 5) {
            snprintf(status + status_len, sizeof(status) - status_len, " ... (%d more)", match_count - 5);
        }
        editor_set_status_msg(ctx, "%s", status);
    }
}

void lua_repl_handle_keypress(editor_ctx_t *ctx, int key) {
    if (!ctx) return;
    t_lua_repl *repl = ctx_repl(ctx);
    if (!repl) return;
    int prompt_len = (int)strlen(LUA_REPL_PROMPT);

    switch(key) {
    case CTRL_L:
    case ESC:
    case CTRL_C:
        repl->active = 0;
        editor_update_repl_layout(ctx);
        editor_set_status_msg(ctx, "Lua REPL closed");
        return;
    case CTRL_U:
        lua_repl_clear_input(repl);
        repl->history_index = -1;
        return;
    case BACKSPACE:
    case CTRL_H:
    case DEL_KEY:
        if (repl->input_len > 0) {
            repl->input[--repl->input_len] = '\0';
        }
        repl->history_index = -1;
        return;
    case ARROW_UP:
        if (repl->history_len > 0) {
            if (repl->history_index == -1)
                repl->history_index = repl->history_len - 1;
            else if (repl->history_index > 0)
                repl->history_index--;
            lua_repl_history_apply(ctx, repl);
        }
        return;
    case ARROW_DOWN:
        if (repl->history_len > 0) {
            if (repl->history_index == -1) {
                return;
            } else if (repl->history_index < repl->history_len - 1) {
                repl->history_index++;
                lua_repl_history_apply(ctx, repl);
            } else {
                repl->history_index = -1;
                lua_repl_clear_input(repl);
            }
        }
        return;
    case ENTER:
        lua_repl_execute_current(ctx);
        if (!repl->active) {
            editor_update_repl_layout(ctx);
        }
        return;
    case TAB:
        lua_repl_complete_input(ctx);
        return;
    default:
        if (isprint(key)) {
            if (repl->input_len < KILO_QUERY_LEN) {
                if (ctx->view.screencols <= prompt_len) return;
                if (prompt_len + repl->input_len >= ctx->view.screencols) return;
                repl->input[repl->input_len++] = key;
                repl->input[repl->input_len] = '\0';
                repl->history_index = -1;
            }
        }
        return;
    }
}

void lua_repl_free(t_lua_repl *repl) {
    for (int i = 0; i < repl->history_len; i++) {
        free(repl->history[i]);
        repl->history[i] = NULL;
    }
    repl->history_len = 0;
    repl->history_index = -1;

    lua_repl_reset_log(repl);
}

void lua_repl_init(t_lua_repl *repl) {
    lua_repl_free(repl);
    repl->active = 0;
    repl->history_index = -1;
    lua_repl_clear_input(repl);
}

/* ======================= LuaHost Lifecycle Functions ======================= */

LuaHost *lua_host_create(void) {
    LuaHost *host = calloc(1, sizeof(LuaHost));
    if (!host) return NULL;
    host->L = NULL;
    memset(&host->repl, 0, sizeof(t_lua_repl));
    return host;
}

void lua_host_free(LuaHost *host) {
    if (!host) return;
    lua_repl_free(&host->repl);
    if (host->L) {
        lua_close(host->L);
        host->L = NULL;
    }
    free(host);
}

void lua_host_init_repl(LuaHost *host) {
    if (!host) return;
    lua_repl_init(&host->repl);
}
