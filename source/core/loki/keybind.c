/* keybind.c - Command dispatcher for configurable keybindings
 *
 * Maps key codes to commands via TOML configuration.
 */

#include "keybind.h"
#include "internal.h"
#include "config.h"
#include "lang_bridge.h"
#include "selection.h"
#include "modal.h"
#include "search.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Maximum registered commands */
#define MAX_COMMANDS 64

/* Command registry entry */
typedef struct {
    const char *name;
    keybind_handler_t handler;
} command_entry_t;

/* Command registry */
static command_entry_t g_commands[MAX_COMMANDS];
static int g_command_count = 0;
static int g_initialized = 0;

/* ======================= Key Code Mapping ======================= */

/* Map key codes to string names */
static const struct {
    int code;
    const char *name;
} key_map[] = {
    { CTRL_C, "ctrl-c" },
    { CTRL_D, "ctrl-d" },
    { CTRL_E, "ctrl-e" },
    { CTRL_F, "ctrl-f" },
    { CTRL_G, "ctrl-g" },
    { CTRL_H, "ctrl-h" },
    { CTRL_L, "ctrl-l" },
    { CTRL_P, "ctrl-p" },
    { CTRL_Q, "ctrl-q" },
    { CTRL_S, "ctrl-s" },
    { CTRL_T, "ctrl-t" },
    { CTRL_U, "ctrl-u" },
    { CTRL_W, "ctrl-w" },
    { CTRL_X, "ctrl-x" },
    { 0, NULL }
};

const char *keybind_key_to_string(int keycode) {
    for (int i = 0; key_map[i].name; i++) {
        if (key_map[i].code == keycode) {
            return key_map[i].name;
        }
    }
    return NULL;
}

/* ======================= Command Lookup ======================= */

const char *keybind_get_command(int keycode) {
    const char *key_str = keybind_key_to_string(keycode);
    if (!key_str) return NULL;

    loki_config_t *config = config_global();
    return config_get_keybinding(config, key_str);
}

/* ======================= Built-in Command Handlers ======================= */

/* Forward declarations for editor functions */
int editor_save(editor_ctx_t *ctx);
void editor_update_repl_layout(editor_ctx_t *ctx);
void copy_selection_to_clipboard(editor_ctx_t *ctx);
int buffer_create(const char *filename);
void buffer_switch(int id);

/* Command: save */
static int cmd_save(editor_ctx_t *ctx, int fd) {
    (void)fd;
    editor_save(ctx);
    return 1;
}

/* Note: quit is handled specially in modal.c due to multi-press confirmation */

/* Command: find (keys then reach search through modal_process_event) */
static int cmd_find(editor_ctx_t *ctx, int fd) {
    (void)fd;
    search_begin(ctx);
    return 1;
}

/* Note: eval_line and play_file are complex commands handled directly in modal.c
 * because they need access to internal functions like get_current_part() */

/* Command: stop (stop all playback) */
static int cmd_stop(editor_ctx_t *ctx, int fd) {
    (void)fd;

#ifdef BUILD_CSOUND_BACKEND
    extern void shared_csound_stop_playback(void);
    shared_csound_stop_playback();
#endif

    loki_lang_stop_all(ctx);
    editor_set_status_msg(ctx, "Playback stopped");
    return 1;
}

/* Command: lua_repl (toggle Lua REPL) */
static int cmd_lua_repl(editor_ctx_t *ctx, int fd) {
    (void)fd;
    if (ctx_repl(ctx)) {
        ctx_repl(ctx)->active = !ctx_repl(ctx)->active;
        editor_update_repl_layout(ctx);
        if (ctx_repl(ctx)->active) {
            editor_set_status_msg(ctx, "Lua REPL active (Ctrl-L or ESC to close)");
        }
    } else {
        editor_set_status_msg(ctx, "Lua is disabled (set editor.lua.enabled = true in config.toml)");
    }
    return 1;
}

/* Command: new_buffer */
static int cmd_new_buffer(editor_ctx_t *ctx, int fd) {
    (void)fd;
    (void)ctx;
    int new_id = buffer_create(NULL);
    if (new_id >= 0) {
        buffer_switch(new_id);
        editor_set_status_msg(ctx, "Created buffer %d", new_id);
    } else {
        editor_set_status_msg(ctx, "Error: Could not create buffer");
    }
    return 1;
}

/* Command: copy */
static int cmd_copy(editor_ctx_t *ctx, int fd) {
    (void)fd;
    copy_selection_to_clipboard(ctx);
    return 1;
}

/* Command: word_wrap (toggle) */
static int cmd_word_wrap(editor_ctx_t *ctx, int fd) {
    (void)fd;
    ctx->view.word_wrap = !ctx->view.word_wrap;
    editor_set_status_msg(ctx, "Word wrap %s", ctx->view.word_wrap ? "enabled" : "disabled");
    return 1;
}

/* Note: link_toggle uses :link command which is handled by command.c */

/* ======================= Command Registration ======================= */

int keybind_register(const char *command, keybind_handler_t handler) {
    if (g_command_count >= MAX_COMMANDS) {
        return -1;
    }

    /* Check for duplicate */
    for (int i = 0; i < g_command_count; i++) {
        if (strcmp(g_commands[i].name, command) == 0) {
            g_commands[i].handler = handler;  /* Update existing */
            return 0;
        }
    }

    g_commands[g_command_count].name = command;
    g_commands[g_command_count].handler = handler;
    g_command_count++;
    return 0;
}

void keybind_init(void) {
    if (g_initialized) return;

    /* Register built-in commands */
    keybind_register("save", cmd_save);
    keybind_register("find", cmd_find);
    keybind_register("stop", cmd_stop);
    keybind_register("lua_repl", cmd_lua_repl);
    keybind_register("new_buffer", cmd_new_buffer);
    keybind_register("copy", cmd_copy);
    keybind_register("word_wrap", cmd_word_wrap);

    /* Register modal commands (defined in modal.c) */
    keybind_register("eval_line", cmd_eval_line);
    keybind_register("play_file", cmd_play_file);
    keybind_register("quit", cmd_quit_keybind);

    g_initialized = 1;
}

/* ======================= Command Execution ======================= */

int keybind_execute(editor_ctx_t *ctx, int fd, const char *command) {
    if (!command) return 0;

    /* Initialize if needed */
    if (!g_initialized) {
        keybind_init();
    }

    /* Look up handler */
    for (int i = 0; i < g_command_count; i++) {
        if (strcmp(g_commands[i].name, command) == 0) {
            return g_commands[i].handler(ctx, fd);
        }
    }

    return 0;  /* Command not found */
}

int keybind_try_handle(editor_ctx_t *ctx, int fd, int keycode) {
    const char *command = keybind_get_command(keycode);
    if (!command) return 0;

    return keybind_execute(ctx, fd, command);
}
