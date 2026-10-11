/* loki_modal.c - Modal editing (vim-like modes)
 *
 * This module implements vim-like modal editing with three modes:
 * - NORMAL mode: Navigation and commands (default)
 * - INSERT mode: Text insertion
 * - VISUAL mode: Text selection
 *
 * Modal editing separates navigation from text insertion, allowing
 * efficient keyboard-only editing without modifier keys.
 *
 * Keybindings:
 * NORMAL mode:
 *   h/j/k/l - Move cursor left/down/up/right
 *   i - Enter INSERT mode
 *   a - Enter INSERT mode after cursor
 *   o/O - Insert line below/above and enter INSERT mode
 *   v - Enter VISUAL mode (selection)
 *   x - Delete character
 *   {/} - Paragraph motion (move to prev/next empty line)
 *
 * INSERT mode:
 *   ESC - Return to NORMAL mode
 *   Normal typing inserts characters
 *   Arrow keys move cursor
 *
 * VISUAL mode:
 *   h/j/k/l - Extend selection
 *   y - Yank (copy) selection
 *   ESC - Return to NORMAL mode
 */

#include "modal.h"
#include "internal.h"
#include "selection.h"
#include "search.h"
#include "command.h"
#include "terminal.h"
#include "undo.h"
#include "buffers.h"
#include "lang_bridge.h"
#include "keybind.h"
#include "picker.h"
#include "window.h"
#ifdef BUILD_CSOUND_BACKEND
#include "shared/audio/audio.h"  /* For CSD file playback */
#endif
#include <stdlib.h>
#include <string.h>

/* Lua headers for keymap dispatch */
#include "lua.h"
#include "lauxlib.h"

/* Control key definition (if not already defined) */
#ifndef CTRL_R
#define CTRL_R 18
#endif

/* Number of times CTRL-Q must be pressed before actually quitting */

/* Helper: check if a filename has .csd extension */
static int is_csd_file(const char *filename) {
    if (!filename) return 0;
    size_t len = strlen(filename);
    if (len < 4) return 0;
    return strcmp(filename + len - 4, ".csd") == 0;
}

/* Try to dispatch a keypress to a Lua keymap callback.
 * Checks _loki_keymaps.{mode}[keycode] for a registered function.
 * Returns 1 if handled by Lua, 0 if not (fall through to built-in). */
static int try_lua_keymap(editor_ctx_t *ctx, const char *mode, int key) {
    lua_State *L = ctx_L(ctx);
    if (!L) return 0;

    /* Get _loki_keymaps global table */
    lua_getglobal(L, "_loki_keymaps");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return 0;
    }

    /* Get mode subtable (e.g., _loki_keymaps.normal) */
    lua_getfield(L, -1, mode);
    if (!lua_istable(L, -1)) {
        lua_pop(L, 2);
        return 0;
    }

    /* Get callback function at mode_table[keycode] */
    lua_pushinteger(L, key);
    lua_gettable(L, -2);
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 3);  /* Pop nil/value, mode_table, _loki_keymaps */
        return 0;
    }

    /* Found a Lua keymap - call it */
    int pcall_result = lua_pcall(L, 0, 0, 0);
    if (pcall_result != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        editor_set_status_msg(ctx, "Lua error: %s", err ? err : "(no message)");
        lua_pop(L, 1);  /* Pop error message */
    }

    lua_pop(L, 2);  /* Pop mode_table and _loki_keymaps */
    return 1;  /* Handled by Lua */
}

/* Dispatch a normal-mode key to commands registered from Lua. A global
 * loki_process_normal_key(key) takes precedence; without one, the key is
 * looked up in loki.register_command()'s _loki_commands table. Returns 1 if
 * Lua handled the key. */
static int try_lua_normal_command(editor_ctx_t *ctx, int key) {
    lua_State *L = ctx_L(ctx);
    if (!L) return 0;

    lua_getglobal(L, "loki_process_normal_key");
    if (!lua_isfunction(L, -1)) {
        lua_pop(L, 1);
        if (key < 32 || key > 126) return 0;   /* registry keys are printable */

        lua_getglobal(L, "_loki_commands");
        if (!lua_istable(L, -1)) {
            lua_pop(L, 1);
            return 0;
        }
        char name[2] = { (char)key, '\0' };
        lua_getfield(L, -1, name);
        if (!lua_isfunction(L, -1)) {
            lua_pop(L, 2);
            return 0;
        }
        if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
            const char *err = lua_tostring(L, -1);
            editor_set_status_msg(ctx, "Command error: %s", err ? err : "(no message)");
            lua_pop(L, 1);
        }
        lua_pop(L, 1);  /* _loki_commands */
        return 1;
    }

    lua_pushinteger(L, key);
    if (lua_pcall(L, 1, 1, 0) != LUA_OK) {
        const char *err = lua_tostring(L, -1);
        editor_set_status_msg(ctx, "Lua error: %s", err ? err : "(no message)");
        lua_pop(L, 1);
        return 0;
    }

    int handled = lua_toboolean(L, -1);
    lua_pop(L, 1);
    return handled;
}

/* Helper: Check if a line is empty (blank or whitespace only) */
static int is_empty_line(editor_ctx_t *ctx, int row) {
    if (row < 0 || row >= ctx->model.numrows) return 1;
    t_erow *line = &ctx->model.row[row];
    for (int i = 0; i < line->size; i++) {
        if (line->chars[i] != ' ' && line->chars[i] != '\t') {
            return 0;
        }
    }
    return 1;
}

/* Move to next empty line (paragraph motion: }) */
static void move_to_next_empty_line(editor_ctx_t *ctx) {
    int filerow = ctx->view.rowoff + ctx->view.cy;

    /* Skip current paragraph (non-empty lines) */
    int row = filerow + 1;
    while (row < ctx->model.numrows && !is_empty_line(ctx, row)) {
        row++;
    }

    /* Skip empty lines to find start of next paragraph or stay at first empty */
    if (row < ctx->model.numrows) {
        /* Found an empty line - this is where we stop */
        filerow = row;
    } else {
        /* No empty line found, go to end of file. Clamp: an empty buffer has
         * numrows == 0, and a negative filerow poisons rowoff/cy. */
        filerow = ctx->model.numrows > 0 ? ctx->model.numrows - 1 : 0;
    }

    /* Update cursor position */
    if (filerow < ctx->view.rowoff) {
        ctx->view.rowoff = filerow;
        ctx->view.cy = 0;
    } else if (filerow >= ctx->view.rowoff + ctx->view.screenrows) {
        ctx->view.rowoff = filerow - ctx->view.screenrows + 1;
        ctx->view.cy = ctx->view.screenrows - 1;
    } else {
        ctx->view.cy = filerow - ctx->view.rowoff;
    }

    /* Move to start of line */
    ctx->view.cx = 0;
    ctx->view.coloff = 0;
}

/* Move to previous empty line (paragraph motion: {) */
static void move_to_prev_empty_line(editor_ctx_t *ctx) {
    int filerow = ctx->view.rowoff + ctx->view.cy;

    /* Skip current paragraph (non-empty lines) going backward */
    int row = filerow - 1;
    while (row >= 0 && !is_empty_line(ctx, row)) {
        row--;
    }

    /* Found an empty line - this is where we stop */
    if (row >= 0) {
        filerow = row;
    } else {
        /* No empty line found, go to start of file */
        filerow = 0;
    }

    /* Update cursor position */
    if (filerow < ctx->view.rowoff) {
        ctx->view.rowoff = filerow;
        ctx->view.cy = 0;
    } else if (filerow >= ctx->view.rowoff + ctx->view.screenrows) {
        ctx->view.rowoff = filerow - ctx->view.screenrows + 1;
        ctx->view.cy = ctx->view.screenrows - 1;
    } else {
        ctx->view.cy = filerow - ctx->view.rowoff;
    }

    /* Move to start of line */
    ctx->view.cx = 0;
    ctx->view.coloff = 0;
}

/* Check if a line is an Alda part declaration (e.g., "piano:", "trumpet/trombone:")
 * Returns 1 if the line contains a part declaration, 0 otherwise.
 * Pattern: optional whitespace, then identifier chars, then ':' not inside quotes */
static int is_part_declaration(const char *line, int len) {
    if (!line || len <= 0) return 0;

    int i = 0;
    /* Skip leading whitespace */
    while (i < len && (line[i] == ' ' || line[i] == '\t')) i++;

    /* Must start with a letter (instrument names start with letters) */
    if (i >= len || !((line[i] >= 'a' && line[i] <= 'z') ||
                      (line[i] >= 'A' && line[i] <= 'Z'))) {
        return 0;
    }

    /* Scan for ':' - valid chars are letters, digits, _-+'()/" and space (for aliases) */
    int in_quotes = 0;
    while (i < len) {
        char c = line[i];
        if (c == '"') {
            in_quotes = !in_quotes;
        } else if (c == ':' && !in_quotes) {
            /* Found unquoted colon - this is a part declaration */
            return 1;
        } else if (!in_quotes) {
            /* Outside quotes: only allow valid instrument/alias chars */
            int valid = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_' || c == '-' ||
                        c == '+' || c == '\'' || c == '(' || c == ')' ||
                        c == '/' || c == ' ' || c == '.';
            if (!valid) return 0;
        }
        i++;
    }

    return 0;  /* No colon found */
}

/* Get the Alda part containing the cursor position.
 * A part starts at a line with an instrument declaration (e.g., "piano:")
 * and extends until the next part declaration or EOF.
 * Returns newly allocated string, caller must free. Returns NULL on error. */
static char *get_current_part(editor_ctx_t *ctx) {
    if (!ctx || ctx->model.numrows == 0) return NULL;

    int cursor_row = ctx->view.rowoff + ctx->view.cy;
    if (cursor_row >= ctx->model.numrows) cursor_row = ctx->model.numrows - 1;

    /* Find start of part: scan backward to find part declaration */
    int start_row = cursor_row;
    while (start_row > 0) {
        t_erow *row = &ctx->model.row[start_row];
        if (is_part_declaration(row->chars, row->size)) {
            break;  /* Found part declaration */
        }
        start_row--;
    }

    /* Find end of part: scan forward to find next part declaration */
    int end_row = cursor_row + 1;
    while (end_row < ctx->model.numrows) {
        t_erow *row = &ctx->model.row[end_row];
        if (is_part_declaration(row->chars, row->size)) {
            break;  /* Found next part */
        }
        end_row++;
    }
    /* end_row is exclusive (first row of next part, or numrows) */

    /* Calculate total length needed */
    size_t total_len = 0;
    for (int i = start_row; i < end_row; i++) {
        total_len += ctx->model.row[i].size + 1;  /* +1 for newline */
    }

    char *result = malloc(total_len + 1);
    if (!result) return NULL;

    /* Concatenate all lines */
    char *p = result;
    for (int i = start_row; i < end_row; i++) {
        memcpy(p, ctx->model.row[i].chars, ctx->model.row[i].size);
        p += ctx->model.row[i].size;
        *p++ = '\n';
    }
    *p = '\0';

    return result;
}

/* Process normal mode keypresses */
static void process_normal_mode(editor_ctx_t *ctx, int fd, int c) {
    /* Check Lua keymaps first */
    if (try_lua_keymap(ctx, "normal", c)) {
        return;  /* Handled by Lua callback */
    }

    /* Then commands registered via modal.register_command(). */
    if (try_lua_normal_command(ctx, c)) {
        return;  /* Handled by Lua callback */
    }

    switch(c) {
        case 'h': editor_move_cursor(ctx, ARROW_LEFT); break;
        case 'j': editor_move_cursor(ctx, ARROW_DOWN); break;
        case 'k': editor_move_cursor(ctx, ARROW_UP); break;
        case 'l': editor_move_cursor(ctx, ARROW_RIGHT); break;

        /* Paragraph motion */
        case '{':
            move_to_prev_empty_line(ctx);
            break;
        case '}':
            move_to_next_empty_line(ctx);
            break;

        /* Enter insert mode */
        case 'i':
            undo_break_group(ctx);  /* Break undo group on mode change */
            ctx->view.mode = MODE_INSERT;
            break;
        case 'a': {
            undo_break_group(ctx);  /* Break undo group on mode change */
            /* Step right to append after the cursor, but not past the end of
             * the line: ARROW_RIGHT wraps to the next row there, which put
             * the insertion point on the wrong line. */
            int arow = ctx->view.rowoff + ctx->view.cy;
            int acol = ctx->view.coloff + ctx->view.cx;
            if (arow >= 0 && arow < ctx->model.numrows &&
                acol < ctx->model.row[arow].size) {
                editor_move_cursor(ctx, ARROW_RIGHT);
            }
            ctx->view.mode = MODE_INSERT;
            break;
        }
        case 'o':
            /* Insert line below and enter insert mode */
            if (ctx->model.numrows > 0) {
                int filerow = ctx->view.rowoff + ctx->view.cy;
                if (filerow < ctx->model.numrows) {
                    ctx->view.cx = ctx->model.row[filerow].size; /* Move to end of line */
                }
            }
            editor_insert_newline(ctx);
            ctx->view.mode = MODE_INSERT;
            break;
        case 'O':
            /* Insert line above and enter insert mode */
            ctx->view.cx = 0; /* Move to start of line */
            editor_insert_newline(ctx);
            editor_move_cursor(ctx, ARROW_UP);
            ctx->view.mode = MODE_INSERT;
            break;

        /* Enter visual mode */
        case 'v':
            ctx->view.mode = MODE_VISUAL;
            ctx->view.sel_active = 1;
            /* Store selection in file coordinates (not screen coordinates) */
            ctx->view.sel_start_x = ctx->view.coloff + ctx->view.cx;
            ctx->view.sel_start_y = ctx->view.rowoff + ctx->view.cy;
            ctx->view.sel_end_x = ctx->view.coloff + ctx->view.cx;
            ctx->view.sel_end_y = ctx->view.rowoff + ctx->view.cy;
            break;

        /* Enter command mode */
        case ':':
            command_mode_enter(ctx);
            break;

        /* Delete character */
        case 'x':
            editor_del_char(ctx);
            break;

        /* Undo/Redo */
        case 'u':
            if (undo_perform(ctx)) {
                editor_set_status_msg(ctx, "Undo");
            } else {
                editor_set_status_msg(ctx, "Already at oldest change");
            }
            break;
        case CTRL_R:
            if (redo_perform(ctx)) {
                editor_set_status_msg(ctx, "Redo");
            } else {
                editor_set_status_msg(ctx, "Already at newest change");
            }
            break;

        /* Global commands (work in all modes) */
        case CTRL_S: editor_save(ctx); break;
        case CTRL_F:
            /* Find requires terminal I/O - handled by modal_process_keypress().
             * From non-terminal sources (modal_process_event), this is a no-op. */
            if (fd != 0) editor_find(ctx, fd);
            break;
        case CTRL_L:
            /* Toggle REPL */
            if (ctx_repl(ctx)) {
                ctx_repl(ctx)->active = !ctx_repl(ctx)->active;
                editor_update_repl_layout(ctx);
                if (ctx_repl(ctx)->active) {
                    editor_set_status_msg(ctx, "Lua REPL active (Ctrl-L or ESC to close)");
                }
            }
            break;
        case CTRL_E:
            cmd_eval_line(ctx, fd);
            break;
        case CTRL_P:
            cmd_play_file(ctx, fd);
            break;
        case CTRL_G:
            /* Stop playback */
            {
#ifdef BUILD_CSOUND_BACKEND
                /* Handle CSD file stop separately (Csound backend) */
                if (is_csd_file(ctx->model.filename)) {
                    if (shared_csound_playback_active()) {
                        shared_csound_stop_playback();
                        editor_set_status_msg(ctx, "Stopped");
                        break;
                    }
                }
#endif
                /* Stop all language playback */
                loki_lang_stop_all(ctx);
                editor_set_status_msg(ctx, "Stopped");
            }
            break;
        case CTRL_Q:
            /* Handle quit in main function for consistency */
            break;

        /* Arrow keys */
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editor_move_cursor(ctx, c);
            break;

        default:
            /* Beep or show message for unknown command */
            editor_set_status_msg(ctx, "Unknown command");
            break;
    }
}

/* Process insert mode keypresses */
static void process_insert_mode(editor_ctx_t *ctx, int fd, int c) {
    /* Check Lua keymaps first */
    if (try_lua_keymap(ctx, "insert", c)) {
        return;  /* Handled by Lua callback */
    }

    switch(c) {
        case ESC:
            ctx->view.mode = MODE_NORMAL;
            /* Move cursor left if not at start of line */
            if (ctx->view.cx > 0 || ctx->view.coloff > 0) {
                editor_move_cursor(ctx, ARROW_LEFT);
            }
            break;

        case ENTER:
            editor_insert_newline(ctx);
            break;

        case BACKSPACE:
        case CTRL_H:
        case DEL_KEY:
            editor_del_char(ctx);
            break;

        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editor_move_cursor(ctx, c);
            break;

        /* Global commands */
        case CTRL_S: editor_save(ctx); break;
        case CTRL_F:
            /* Find requires terminal I/O - handled by modal_process_keypress().
             * From non-terminal sources (modal_process_event), this is a no-op. */
            if (fd != 0) editor_find(ctx, fd);
            break;
        /* CTRL_W is now a prefix key for window commands, handled in modal_process_event */
        case CTRL_L:
            /* Toggle REPL */
            if (ctx_repl(ctx)) {
                ctx_repl(ctx)->active = !ctx_repl(ctx)->active;
                editor_update_repl_layout(ctx);
                if (ctx_repl(ctx)->active) {
                    editor_set_status_msg(ctx, "Lua REPL active (Ctrl-L or ESC to close)");
                }
            }
            break;
        case CTRL_C:
            copy_selection_to_clipboard(ctx);
            break;
        case CTRL_E:
            cmd_eval_line(ctx, fd);
            break;
        case CTRL_P:
            cmd_play_file(ctx, fd);
            break;
        case CTRL_G:
            /* Stop playback */
            {
#ifdef BUILD_CSOUND_BACKEND
                /* Handle CSD file stop separately (Csound backend) */
                if (is_csd_file(ctx->model.filename)) {
                    if (shared_csound_playback_active()) {
                        shared_csound_stop_playback();
                        editor_set_status_msg(ctx, "Stopped");
                        break;
                    }
                }
#endif
                /* Stop all language playback */
                loki_lang_stop_all(ctx);
                editor_set_status_msg(ctx, "Stopped");
            }
            break;

        case PAGE_UP:
        case PAGE_DOWN:
            if (c == PAGE_UP && ctx->view.cy != 0)
                ctx->view.cy = 0;
            else if (c == PAGE_DOWN && ctx->view.cy != ctx->view.screenrows-1)
                ctx->view.cy = ctx->view.screenrows-1;
            {
                int times = ctx->view.screenrows;
                while(times--)
                    editor_move_cursor(ctx, c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            break;

        case SHIFT_ARROW_UP:
        case SHIFT_ARROW_DOWN:
        case SHIFT_ARROW_LEFT:
        case SHIFT_ARROW_RIGHT:
            /* Start selection if not active */
            if (!ctx->view.sel_active) {
                ctx->view.sel_active = 1;
                ctx->view.sel_start_x = ctx->view.coloff + ctx->view.cx;
                ctx->view.sel_start_y = ctx->view.rowoff + ctx->view.cy;
            }
            /* Move cursor */
            if (c == SHIFT_ARROW_UP) editor_move_cursor(ctx, ARROW_UP);
            else if (c == SHIFT_ARROW_DOWN) editor_move_cursor(ctx, ARROW_DOWN);
            else if (c == SHIFT_ARROW_LEFT) editor_move_cursor(ctx, ARROW_LEFT);
            else if (c == SHIFT_ARROW_RIGHT) editor_move_cursor(ctx, ARROW_RIGHT);
            /* Update selection end */
            ctx->view.sel_end_x = ctx->view.coloff + ctx->view.cx;
            ctx->view.sel_end_y = ctx->view.rowoff + ctx->view.cy;
            break;

        default:
            /* Insert the character */
            editor_insert_char(ctx, c);
            break;
    }
}

/* Process visual mode keypresses */
static void process_visual_mode(editor_ctx_t *ctx, int fd, int c) {
    /* Check Lua keymaps first */
    if (try_lua_keymap(ctx, "visual", c)) {
        return;  /* Handled by Lua callback */
    }

    switch(c) {
        case ESC:
            ctx->view.mode = MODE_NORMAL;
            ctx->view.sel_active = 0;
            break;

        /* Movement extends selection */
        case 'h':
        case ARROW_LEFT:
            editor_move_cursor(ctx, ARROW_LEFT);
            /* Update selection end in file coordinates */
            ctx->view.sel_end_x = ctx->view.coloff + ctx->view.cx;
            ctx->view.sel_end_y = ctx->view.rowoff + ctx->view.cy;
            break;

        case 'j':
        case ARROW_DOWN:
            editor_move_cursor(ctx, ARROW_DOWN);
            /* Update selection end in file coordinates */
            ctx->view.sel_end_x = ctx->view.coloff + ctx->view.cx;
            ctx->view.sel_end_y = ctx->view.rowoff + ctx->view.cy;
            break;

        case 'k':
        case ARROW_UP:
            editor_move_cursor(ctx, ARROW_UP);
            /* Update selection end in file coordinates */
            ctx->view.sel_end_x = ctx->view.coloff + ctx->view.cx;
            ctx->view.sel_end_y = ctx->view.rowoff + ctx->view.cy;
            break;

        case 'l':
        case ARROW_RIGHT:
            editor_move_cursor(ctx, ARROW_RIGHT);
            /* Update selection end in file coordinates */
            ctx->view.sel_end_x = ctx->view.coloff + ctx->view.cx;
            ctx->view.sel_end_y = ctx->view.rowoff + ctx->view.cy;
            break;

        /* Copy selection */
        case 'y':
            copy_selection_to_clipboard(ctx);
            ctx->view.mode = MODE_NORMAL;
            ctx->view.sel_active = 0;
            editor_set_status_msg(ctx, "Yanked selection");
            break;

        /* Delete selection (yank first for 'd', just delete for 'x') */
        case 'd':
            copy_selection_to_clipboard(ctx); /* Save to clipboard first (yank) */
            {
                int deleted = delete_selection(ctx);
                editor_set_status_msg(ctx, "Deleted %d characters", deleted);
            }
            ctx->view.mode = MODE_NORMAL;
            break;
        case 'x':
            {
                int deleted = delete_selection(ctx);
                editor_set_status_msg(ctx, "Deleted %d characters", deleted);
            }
            ctx->view.mode = MODE_NORMAL;
            break;

        /* Global commands */
        case CTRL_C:
            copy_selection_to_clipboard(ctx);
            break;

        default:
            /* Unknown command - beep */
            editor_set_status_msg(ctx, "Unknown visual command");
            break;
    }
    (void)fd; /* Unused */
}

/* Process a single keypress with modal editing support.
 *
 * This is the terminal-specific entry point for keyboard input.
 * It reads a key from the terminal and delegates to modal_process_event().
 *
 * Terminal-specific operations (like interactive find via Ctrl-F) are
 * intercepted here and handled with the real fd before delegation.
 *
 * For non-terminal input sources, use modal_process_event() directly.
 */
int modal_process_keypress(editor_ctx_t *ctx, int fd) {
    int c = terminal_read_key(fd);
    if (c == -1) return -1;  /* Input closed or failed. */

    /* Handle pending Ctrl-X prefix - read second key immediately from terminal */
    if (ctx->view.pending_prefix == CTRL_X) {
        /* Already have Ctrl-X pending, this key completes the sequence.
         * Convert to event and let modal_process_event() handle it. */
        EditorEvent ev = event_from_keycode(c);
        modal_process_event(ctx, &ev);
        return 0;
    }

    /* Handle pending Ctrl-W prefix - read second key immediately from terminal */
    if (ctx->view.pending_prefix == CTRL_W) {
        /* Already have Ctrl-W pending, this key completes the sequence.
         * Convert to event and let modal_process_event() handle it. */
        EditorEvent ev = event_from_keycode(c);
        modal_process_event(ctx, &ev);
        return 0;
    }

    /* Intercept CTRL_F for interactive find (requires terminal I/O).
     * This must be done before delegation because editor_find() has
     * its own event loop that reads from the terminal. */
    if (c == CTRL_F) {
        t_lua_repl *repl = ctx_repl(ctx);
        if (!repl || !repl->active) {
            editor_find(ctx, fd);
            return 0;
        }
    }

    /* Convert keypress to event and delegate to event handler */
    EditorEvent ev = event_from_keycode(c);
    modal_process_event(ctx, &ev);
    return 0;
}

/* ============================================================================
 * Ctrl-X Prefix Handling
 * ============================================================================
 * Ctrl-X is a prefix key for buffer operations. When received, we set
 * pending_prefix and wait for the next key to complete the command.
 */

/* Handle the second key of a Ctrl-X sequence.
 * Returns 1 if handled, 0 if the key was not a valid Ctrl-X command. */
static int handle_ctrl_x_command(editor_ctx_t *ctx, int c) {
    switch (c) {
        case 'n': {
            /* Next buffer */
            int next_id = buffer_next();
            if (next_id >= 0) {
                editor_set_status_msg(ctx, "Switched to buffer %d", next_id);
            }
            return 1;
        }
        case 'p': {
            /* Previous buffer */
            int prev_id = buffer_prev();
            if (prev_id >= 0) {
                editor_set_status_msg(ctx, "Switched to buffer %d", prev_id);
            }
            return 1;
        }
        case 'k': {
            /* Close buffer */
            int current_id = buffer_get_current_id();
            int result = buffer_close(current_id, 0);
            if (result == 1) {
                editor_set_status_msg(ctx, "Buffer has unsaved changes! Use Ctrl-X K to force close");
            } else if (result == 0) {
                editor_set_status_msg(ctx, "Closed buffer %d", current_id);
            } else {
                editor_set_status_msg(ctx, "Cannot close last buffer");
            }
            return 1;
        }
        case 'K': {
            /* Force close buffer */
            int current_id = buffer_get_current_id();
            int result = buffer_close(current_id, 1);
            if (result == 0) {
                editor_set_status_msg(ctx, "Force closed buffer %d", current_id);
            } else {
                editor_set_status_msg(ctx, "Cannot close last buffer");
            }
            return 1;
        }
        default:
            if (c >= '1' && c <= '9') {
                /* Switch to buffer by number (1-9) */
                int ids[MAX_BUFFERS];
                int count = buffer_get_list(ids);
                int index = c - '1';
                if (index < count) {
                    buffer_switch(ids[index]);
                    editor_set_status_msg(ctx, "Switched to buffer %d", ids[index]);
                } else {
                    editor_set_status_msg(ctx, "Buffer %d not found", index + 1);
                }
                return 1;
            }
            return 0;  /* Not a valid Ctrl-X command */
    }
}

/* ============================================================================
 * Ctrl-W Prefix Handling
 * ============================================================================
 * Ctrl-W is a prefix key for window operations. When received, we set
 * pending_prefix and wait for the next key to complete the command.
 */

/* Handle the second key of a Ctrl-W sequence.
 * Returns 1 if handled, 0 if the key was not a valid Ctrl-W command. */
static int handle_ctrl_w_command(editor_ctx_t *ctx, int c) {
    WindowManager *wm = ctx->wm;

    switch (c) {
        case 's': {
            /* Split horizontally (top/bottom) */
            /* Create window manager on first split */
            if (!wm) {
                int model_id = buffer_get_current_id();
                ctx->wm = window_manager_create(model_id);
                wm = ctx->wm;
                if (!wm) {
                    editor_set_status_msg(ctx, "Cannot create window manager");
                    return 1;
                }
                /* Initialize first pane's view from current context */
                if (wm->active && !wm->active->view) {
                    wm->active->view = window_view_clone(&ctx->view);
                }
            }
            /* Save current view state before split */
            window_sync_view_from_ctx(ctx, wm->active);
            if (window_split(wm, SPLIT_HORIZONTAL) == 0) {
                /* Sync new active pane's view to ctx */
                window_sync_view_to_ctx(ctx, wm->active);
                editor_set_status_msg(ctx, "Split horizontal");
            } else {
                editor_set_status_msg(ctx, "Cannot split window");
            }
            return 1;
        }
        case 'v': {
            /* Split vertically (left/right) */
            /* Create window manager on first split */
            if (!wm) {
                int model_id = buffer_get_current_id();
                ctx->wm = window_manager_create(model_id);
                wm = ctx->wm;
                if (!wm) {
                    editor_set_status_msg(ctx, "Cannot create window manager");
                    return 1;
                }
                /* Initialize first pane's view from current context */
                if (wm->active && !wm->active->view) {
                    wm->active->view = window_view_clone(&ctx->view);
                }
            }
            /* Save current view state before split */
            window_sync_view_from_ctx(ctx, wm->active);
            if (window_split(wm, SPLIT_VERTICAL) == 0) {
                /* Sync new active pane's view to ctx */
                window_sync_view_to_ctx(ctx, wm->active);
                editor_set_status_msg(ctx, "Split vertical");
            } else {
                editor_set_status_msg(ctx, "Cannot split window");
            }
            return 1;
        }
        case 'h': {
            /* Navigate left */
            if (!wm || window_count_panes(wm) <= 1) {
                editor_set_status_msg(ctx, "Only one window");
                return 1;
            }
            /* Save current view state before switching */
            window_sync_view_from_ctx(ctx, wm->active);
            if (window_navigate(wm, 'h') == 0) {
                /* Load new active pane's view */
                window_sync_view_to_ctx(ctx, wm->active);
                editor_set_status_msg(ctx, "");
            } else {
                editor_set_status_msg(ctx, "No window to the left");
            }
            return 1;
        }
        case 'j': {
            /* Navigate down */
            if (!wm || window_count_panes(wm) <= 1) {
                editor_set_status_msg(ctx, "Only one window");
                return 1;
            }
            /* Save current view state before switching */
            window_sync_view_from_ctx(ctx, wm->active);
            if (window_navigate(wm, 'j') == 0) {
                /* Load new active pane's view */
                window_sync_view_to_ctx(ctx, wm->active);
                editor_set_status_msg(ctx, "");
            } else {
                editor_set_status_msg(ctx, "No window below");
            }
            return 1;
        }
        case 'k': {
            /* Navigate up */
            if (!wm || window_count_panes(wm) <= 1) {
                editor_set_status_msg(ctx, "Only one window");
                return 1;
            }
            /* Save current view state before switching */
            window_sync_view_from_ctx(ctx, wm->active);
            if (window_navigate(wm, 'k') == 0) {
                /* Load new active pane's view */
                window_sync_view_to_ctx(ctx, wm->active);
                editor_set_status_msg(ctx, "");
            } else {
                editor_set_status_msg(ctx, "No window above");
            }
            return 1;
        }
        case 'l': {
            /* Navigate right */
            if (!wm || window_count_panes(wm) <= 1) {
                editor_set_status_msg(ctx, "Only one window");
                return 1;
            }
            /* Save current view state before switching */
            window_sync_view_from_ctx(ctx, wm->active);
            if (window_navigate(wm, 'l') == 0) {
                /* Load new active pane's view */
                window_sync_view_to_ctx(ctx, wm->active);
                editor_set_status_msg(ctx, "");
            } else {
                editor_set_status_msg(ctx, "No window to the right");
            }
            return 1;
        }
        case 'c': {
            /* Close current pane */
            if (!wm || window_count_panes(wm) <= 1) {
                editor_set_status_msg(ctx, "Cannot close last window");
                return 1;
            }
            if (window_close(wm) == 0) {
                /* Load new active pane's view */
                window_sync_view_to_ctx(ctx, wm->active);
                editor_set_status_msg(ctx, "Closed window");
            } else {
                editor_set_status_msg(ctx, "Cannot close last window");
            }
            return 1;
        }
        case 'w': {
            /* Cycle to next pane */
            if (!wm || window_count_panes(wm) <= 1) {
                editor_set_status_msg(ctx, "Only one window");
                return 1;
            }
            /* Save current view state before switching */
            window_sync_view_from_ctx(ctx, wm->active);
            if (window_cycle(wm) == 0) {
                /* Load new active pane's view */
                window_sync_view_to_ctx(ctx, wm->active);
                editor_set_status_msg(ctx, "");
            } else {
                editor_set_status_msg(ctx, "Only one window");
            }
            return 1;
        }
        case '=': {
            /* Equalize split ratios */
            if (!wm || window_count_panes(wm) <= 1) {
                editor_set_status_msg(ctx, "Only one window");
                return 1;
            }
            window_equalize(wm);
            editor_set_status_msg(ctx, "Windows equalized");
            return 1;
        }
        case 'n': {
            /* Create new buffer in current pane */
            if (!wm || !wm->active) {
                editor_set_status_msg(ctx, "No active window");
                return 1;
            }
            int new_id = buffer_create(NULL);  /* Create empty buffer */
            if (new_id >= 0) {
                /* Update pane to use new buffer */
                wm->active->model_id = new_id;
                /* Reset view for new buffer */
                if (wm->active->view) {
                    wm->active->view->cx = 0;
                    wm->active->view->cy = 0;
                    wm->active->view->rowoff = 0;
                    wm->active->view->coloff = 0;
                    wm->active->view->sel_active = 0;
                }
                window_sync_view_to_ctx(ctx, wm->active);
                editor_set_status_msg(ctx, "New buffer in pane");
            } else {
                editor_set_status_msg(ctx, "Cannot create buffer");
            }
            return 1;
        }
        case 'b': {
            /* Switch to next buffer in current pane */
            if (!wm || !wm->active) {
                editor_set_status_msg(ctx, "No active window");
                return 1;
            }
            /* Get list of buffer IDs */
            int ids[MAX_BUFFERS];
            int count = buffer_get_list(ids);
            if (count <= 1) {
                editor_set_status_msg(ctx, "Only one buffer");
                return 1;
            }
            /* Find current buffer and switch to next */
            int current_id = wm->active->model_id;
            int next_id = ids[0];
            for (int i = 0; i < count; i++) {
                if (ids[i] == current_id) {
                    next_id = ids[(i + 1) % count];
                    break;
                }
            }
            if (next_id != current_id) {
                wm->active->model_id = next_id;
                /* Reset view for switched buffer */
                if (wm->active->view) {
                    wm->active->view->cx = 0;
                    wm->active->view->cy = 0;
                    wm->active->view->rowoff = 0;
                    wm->active->view->coloff = 0;
                    wm->active->view->sel_active = 0;
                }
                window_sync_view_to_ctx(ctx, wm->active);
                const char *name = buffer_get_display_name(next_id);
                editor_set_status_msg(ctx, "Switched to %s", name ? name : "[No Name]");
            }
            return 1;
        }
        default:
            editor_set_status_msg(ctx, "Unknown Ctrl-W command");
            return 0;  /* Not a valid Ctrl-W command */
    }
}

/* ============================================================================
 * Event-Based Entry Point
 * ============================================================================
 * This function provides an event-based interface for modal processing,
 * enabling cleaner modifier handling and test injection.
 */

/**
 * Process an EditorEvent through the modal system.
 *
 * This is the preferred entry point for:
 * - Test code (inject events without terminal I/O)
 * - Future transports (WebSocket, RPC)
 *
 * Handles Ctrl-X prefix sequences via pending_prefix state:
 * - First Ctrl-X sets pending_prefix = CTRL_X
 * - Next event completes the sequence (e.g., 'n' -> next buffer)
 *
 * Note: Interactive operations like editor_find() require terminal I/O.
 * When called from non-terminal sources (fd=0), these are skipped.
 * Use modal_process_keypress() for full terminal functionality.
 */
void modal_process_event(editor_ctx_t *ctx, const EditorEvent *event) {
    if (!ctx || !event) return;

    /* Handle non-key events directly */
    switch (event->type) {
        case EVENT_NONE:
            return;

        case EVENT_QUIT:
            exit(0);

        case EVENT_RESIZE:
            /* Update screen dimensions */
            ctx->view.screenrows = event->data.resize.rows;  /* text rows */
            ctx->view.screencols = event->data.resize.cols;
            editor_refresh_screen(ctx);
            return;

        case EVENT_COMMAND:
            /* Execute ex-command directly */
            if (event->data.command.command) {
                command_execute(ctx, event->data.command.command);
            }
            return;

        case EVENT_ACTION:
            /* Named actions could be dispatched here in the future */
            return;

        case EVENT_KEY:
            /* Fall through to keypress handling */
            break;

        case EVENT_MOUSE:
            /* Handle mouse click to position cursor */
            if (event->data.mouse.pressed && event->data.mouse.button == 0) {
                /* Left click - position cursor */
                int click_row = event->data.mouse.y - 1;  /* 1-based to 0-based */
                int click_col = event->data.mouse.x - 1;

                /* Account for tab bar if multiple buffers */
                int tab_offset = (buffer_count() > 1) ? 1 : 0;
                click_row -= tab_offset;

                /* Account for gutter (line numbers) */
                int gutter_width = 0;
                if (ctx->view.line_numbers && ctx->model.numrows > 0) {
                    int max_line = ctx->model.numrows;
                    gutter_width = 1;
                    while (max_line >= 10) { gutter_width++; max_line /= 10; }
                    gutter_width += 1;
                }
                click_col -= gutter_width;

                if (click_row >= 0 && click_col >= 0) {
                    /* Convert screen position to file position */
                    int file_row = ctx->view.rowoff + click_row;
                    if (file_row < ctx->model.numrows) {
                        ctx->view.cy = click_row;
                        /* Convert screen column to file column (handle tabs) */
                        t_erow *row = &ctx->model.row[file_row];
                        int file_col = 0;
                        int screen_col = 0;
                        while (file_col < row->size && screen_col < click_col + ctx->view.coloff) {
                            if (row->chars[file_col] == '\t') {
                                screen_col += 7 - (screen_col % 8) + 1;  /* Tab width = 8 */
                            } else {
                                screen_col++;
                            }
                            if (screen_col <= click_col + ctx->view.coloff) file_col++;
                        }
                        ctx->view.cx = file_col;
                        if (ctx->view.cx > row->size) ctx->view.cx = row->size;
                    }
                }
            }
            return;
    }

    /* Convert event to legacy keycode for existing handlers */
    int c = event_to_keycode(event);
    if (c == 0) return;

    /* The quit counter lives on the context (ctx->view.quit_times). It used
     * to be a function-level static, shared by every editor context and never
     * reset between sessions. */

    /* Handle pending Ctrl-X prefix sequence */
    if (ctx->view.pending_prefix == CTRL_X) {
        ctx->view.pending_prefix = 0;  /* Clear prefix */
        if (handle_ctrl_x_command(ctx, c)) {
            ctx->view.quit_times = KILO_QUIT_TIMES;
            return;
        }
        /* If not a valid Ctrl-X command, fall through to normal processing */
    }

    /* Handle pending Ctrl-W prefix sequence */
    if (ctx->view.pending_prefix == CTRL_W) {
        ctx->view.pending_prefix = 0;  /* Clear prefix */
        if (handle_ctrl_w_command(ctx, c)) {
            ctx->view.quit_times = KILO_QUIT_TIMES;
            return;
        }
        /* If not a valid Ctrl-W command, fall through to normal processing */
    }

    /* REPL keypress handling */
    if (ctx_repl(ctx) && ctx_repl(ctx)->active) {
        lua_repl_handle_keypress(ctx, c);
        return;
    }

    /* Handle quit globally (works in all modes) */
    if (c == CTRL_Q) {
        if (ctx->model.dirty && ctx->view.quit_times) {
            editor_set_status_msg(ctx, "WARNING!!! File has unsaved changes. "
                "Press Ctrl-Q %d more times to quit.", ctx->view.quit_times);
            ctx->view.quit_times--;
            return;
        }
        exit(0);
    }

    /* Try configurable keybindings first (from config.toml)
     * This allows users to override default keybindings */
    if (keybind_try_handle(ctx, 0, c)) {
        ctx->view.quit_times = KILO_QUIT_TIMES;
        return;
    }

    /* Handle buffer operations globally */
    if (c == CTRL_T) {
        /* Create new buffer */
        int new_id = buffer_create(NULL);
        if (new_id >= 0) {
            buffer_switch(new_id);
            editor_set_status_msg(ctx, "Created buffer %d", new_id);
        } else {
            editor_set_status_msg(ctx, "Error: Could not create buffer (max %d buffers)", MAX_BUFFERS);
        }
        ctx->view.quit_times = KILO_QUIT_TIMES;
        return;
    }

    /* Handle Ctrl-X prefix */
    if (c == CTRL_X) {
        ctx->view.pending_prefix = CTRL_X;
        ctx->view.quit_times = KILO_QUIT_TIMES;
        return;
    }

    /* Handle Ctrl-W prefix */
    if (c == CTRL_W) {
        ctx->view.pending_prefix = CTRL_W;
        ctx->view.quit_times = KILO_QUIT_TIMES;
        return;
    }

    /* Dispatch to mode-specific handler (fd=0, no terminal I/O) */
    switch(ctx->view.mode) {
        case MODE_NORMAL:
            process_normal_mode(ctx, 0, c);
            break;
        case MODE_INSERT:
            process_insert_mode(ctx, 0, c);
            break;
        case MODE_VISUAL:
            process_visual_mode(ctx, 0, c);
            break;
        case MODE_COMMAND:
            command_mode_handle_key(ctx, 0, c);
            break;
        case MODE_PICKER:
            picker_handle_key(ctx, c);
            break;
    }

    ctx->view.quit_times = KILO_QUIT_TIMES; /* Reset it to the original value. */
}

/* ============================================================================
 * Keybind Command Handlers
 * ============================================================================
 * These functions implement commands that can be registered with the keybind
 * system, allowing them to be remapped via TOML configuration.
 */

/* Command: eval_line - evaluate selection or current part */
int cmd_eval_line(editor_ctx_t *ctx, int fd) {
    (void)fd;

    /* Handle .csd files - partial playback not supported */
    if (is_csd_file(ctx->model.filename)) {
        editor_set_status_msg(ctx, "Partial playback not supported for .csd files (use Ctrl-P)");
        return 1;
    }

    /* Get code to evaluate */
    char *code = get_selection_text(ctx);
    if (!code && ctx->model.numrows > 0 && ctx->view.cy < ctx->model.numrows) {
        /* No selection - use current part/line */
        code = get_current_part(ctx);
    }
    if (!code || !*code) {
        editor_set_status_msg(ctx, "No code to evaluate");
        free(code);
        return 1;
    }

    /* Evaluate code with the appropriate language */
    const LokiLangOps *lang = loki_lang_for_file(ctx->model.filename);
    if (lang) {
        int ret = loki_lang_eval(ctx, code);
        if (ret == 0) {
            editor_set_status_msg(ctx, "%s: evaluated", lang->name);
        } else if (ret == -1) {
            const char *err = loki_lang_get_error(ctx);
            editor_set_status_msg(ctx, "%s error: %s", lang->name,
                err ? err : "eval failed");
        }
    } else {
        editor_set_status_msg(ctx, "No language support for this file type");
    }
    free(code);

    /* Clear selection after eval */
    ctx->view.sel_active = 0;
    return 1;
}

/* Command: play_file - play entire file */
int cmd_play_file(editor_ctx_t *ctx, int fd) {
    (void)fd;

#ifdef BUILD_CSOUND_BACKEND
    /* Handle .csd files with Csound backend */
    if (is_csd_file(ctx->model.filename)) {
        if (ctx->model.dirty) {
            editor_set_status_msg(ctx, "Save file first (Ctrl-S) before playing");
            return 1;
        }
        if (!ctx->model.filename) {
            editor_set_status_msg(ctx, "No filename - save file first");
            return 1;
        }
        /* Stop any existing playback */
        shared_csound_stop_playback();
        /* Start async playback */
        int result = shared_csound_play_file_async(ctx->model.filename);
        if (result == 0) {
            editor_set_status_msg(ctx, "Playing %s (Ctrl-G to stop)", ctx->model.filename);
        } else {
            const char *err = shared_csound_get_error();
            editor_set_status_msg(ctx, "Csound error: %s", err ? err : "failed to play");
        }
        return 1;
    }
#endif

    /* Check for empty file */
    if (ctx->model.numrows == 0) {
        editor_set_status_msg(ctx, "Empty file");
        return 1;
    }

    /* Build full buffer content */
    size_t total_len = 0;
    for (int i = 0; i < ctx->model.numrows; i++) {
        total_len += ctx->model.row[i].size + 1; /* +1 for newline */
    }
    char *code = malloc(total_len + 1);
    if (!code) {
        editor_set_status_msg(ctx, "Out of memory");
        return 1;
    }
    char *p = code;
    for (int i = 0; i < ctx->model.numrows; i++) {
        memcpy(p, ctx->model.row[i].chars, ctx->model.row[i].size);
        p += ctx->model.row[i].size;
        *p++ = '\n';
    }
    *p = '\0';

    /* Evaluate full file with the appropriate language */
    const LokiLangOps *lang = loki_lang_for_file(ctx->model.filename);
    if (lang) {
        int ret = loki_lang_eval(ctx, code);
        if (ret == 0) {
            editor_set_status_msg(ctx, "%s: playing file", lang->name);
        } else if (ret == -1) {
            const char *err = loki_lang_get_error(ctx);
            editor_set_status_msg(ctx, "%s error: %s", lang->name,
                err ? err : "eval failed");
        }
    } else {
        editor_set_status_msg(ctx, "No language support for this file type");
    }
    free(code);
    return 1;
}

/* Command: quit via keybind - request quit (multi-press confirmation handled elsewhere) */
int cmd_quit_keybind(editor_ctx_t *ctx, int fd) {
    (void)fd;
    /* Signal quit request - actual confirmation is in modal_process_keypress */
    editor_set_status_msg(ctx, "Quit requested");
    return 0;  /* Return 0 to indicate not fully handled - let caller handle quit logic */
}

/* ============================================================================
 * Test Functions - For unit testing only
 * ============================================================================
 * These functions expose the internal mode handlers for unit testing.
 * They should not be used in production code - only in tests.
 */

void modal_process_normal_mode_key(editor_ctx_t *ctx, int fd, int c) {
    process_normal_mode(ctx, fd, c);
}

void modal_process_insert_mode_key(editor_ctx_t *ctx, int fd, int c) {
    process_insert_mode(ctx, fd, c);
}

void modal_process_visual_mode_key(editor_ctx_t *ctx, int fd, int c) {
    process_visual_mode(ctx, fd, c);
}
