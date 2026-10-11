/* command.c - Vim-like command mode implementation
 *
 * Handles command mode (:w, :q, :set, etc.) for vim-like editing.
 * Commands can be built-in (C functions) or registered from Lua.
 *
 * Command implementations are split into separate files in command/:
 *   - file.c      - :w, :e (file operations)
 *   - basic.c     - :q, :wq, :help, :set (core commands)
 *   - goto.c      - :goto, :<number> (navigation)
 *   - substitute.c - :s/old/new/ (search and replace)
 *   - link.c      - :link (Ableton Link)
 *   - csd.c       - :csd (Csound)
 *   - export.c    - :export (MIDI export)
 *
 * To add a new command:
 *   1. Create a new file in command/ (or add to existing category)
 *   2. Include "command/command_impl.h"
 *   3. Implement cmd_yourcommand(editor_ctx_t *ctx, const char *args)
 *   4. Add declaration to command/command_impl.h
 *   5. Add entry to builtin_commands[] below
 *   6. Add source file to CMakeLists.txt LIBLOKI_SOURCES
 */

#include "command.h"
#include "internal.h"
#include "buffers.h"
#include "command/command_impl.h"
#include <lua.h>

/* Command history storage */
static char *command_history[COMMAND_HISTORY_MAX];
static int command_history_count = 0;

/* Dynamic command registry (for Lua-registered commands) */
#define MAX_DYNAMIC_COMMANDS 100
static command_def_t dynamic_commands[MAX_DYNAMIC_COMMANDS];
static int dynamic_command_count = 0;

/* Built-in command table
 * Format: {name, handler, help_text, min_args, max_args}
 */
static command_def_t builtin_commands[] = {
    /* File operations (file.c) */
    {"w",      cmd_write,       "Write (save) file",              0, -1},
    {"write",  cmd_write,       "Write (save) file",              0, -1},
    {"e",      cmd_edit,        "Edit file",                      1, -1},
    {"edit",   cmd_edit,        "Edit file",                      1, -1},

    /* Basic commands (basic.c) */
    {"q",      cmd_quit,        "Quit editor",                    0, 0},
    {"quit",   cmd_quit,        "Quit editor",                    0, 0},
    {"wq",     cmd_write_quit,  "Write and quit",                 0, -1},
    {"x",      cmd_write_quit,  "Write and quit (if modified)",   0, -1},
    {"q!",     cmd_force_quit,  "Quit without saving",            0, 0},
    {"quit!",  cmd_force_quit,  "Quit without saving",            0, 0},
    {"help",   cmd_help,        "Show help",                      0, 1},
    {"h",      cmd_help,        "Show help",                      0, 1},
    {"set",    cmd_set,         "Set option (wrap, etc)",         0, 2},

    /* Navigation (goto.c) */
    {"goto",   cmd_goto,        "Go to line number",              1, 1},

    /* Audio/MIDI (link.c, csd.c, export.c) */
    {"link",   cmd_link,        "Toggle Ableton Link sync",       0, 1},
    {"csd",    cmd_csd,         "Toggle Csound synthesis",        0, 1},
    {"export", cmd_export,      "Export to MIDI file",            1, -1},

    /* Playback */
    {"play",   cmd_play,        "Play entire buffer",             0, 0},
    {"eval",   cmd_eval,        "Evaluate code or current line",  0, -1},
    {"stop",   cmd_stop,        "Stop all playback and loops",    0, 0},

    /* Live loop (loop.c) */
    {"loop",   cmd_loop,        "Start live loop (beats)",        0, 1},
    {"unloop", cmd_unloop,      "Stop live loop",                 0, 0},

    /* Plugin control (plugin.c) */
    {"plugin", cmd_plugin,      "Plugin preset control",          0, -1},

    /* Theme switching (theme.c) */
    {"theme",  cmd_theme,       "List or switch themes",          0, 1},

    /* Metronome and tempo (metronome.c) */
    {"tap",       cmd_tap,       "Tap to set tempo",              0, 1},
    {"metronome", cmd_metronome, "Toggle metronome",              0, 1},

    {NULL, NULL, NULL, 0, 0}  /* Sentinel */
};

/* ======================== Command State Management ======================== */

void command_mode_init(editor_ctx_t *ctx) {
    /* Command state is part of editor_ctx_t, just reset it */
    memset(ctx->view.cmd_buffer, 0, sizeof(ctx->view.cmd_buffer));
    ctx->view.cmd_length = 0;
    ctx->view.cmd_cursor_pos = 0;
    ctx->view.cmd_history_index = 0;
}

void command_mode_free(editor_ctx_t *ctx) {
    /* Nothing to free currently - command buffer is inline */
    (void)ctx;
}

void command_mode_enter(editor_ctx_t *ctx) {
    ctx->view.mode = MODE_COMMAND;
    ctx->view.cmd_buffer[0] = ':';
    ctx->view.cmd_buffer[1] = '\0';
    ctx->view.cmd_length = 1;
    ctx->view.cmd_cursor_pos = 1;
    ctx->view.cmd_history_index = command_history_count;  /* Start at end of history */
    editor_set_status_msg(ctx, ":");
}

void command_mode_exit(editor_ctx_t *ctx) {
    ctx->view.mode = MODE_NORMAL;
    ctx->view.cmd_length = 0;
    ctx->view.cmd_cursor_pos = 0;
    memset(ctx->view.cmd_buffer, 0, sizeof(ctx->view.cmd_buffer));
    /* Note: the status message is deliberately left alone. It usually holds
     * the result the command just produced ("3L written", "Unknown command:
     * ..."); clearing it here wiped every command's feedback. Callers that
     * abandon the command line clear it themselves. */
}

/* ======================== Command History ======================== */

static void command_history_add(const char *cmd) {
    /* Don't add empty or duplicate commands */
    if (!cmd || !cmd[0]) return;
    if (command_history_count > 0 &&
        strcmp(command_history[command_history_count - 1], cmd) == 0) {
        return;
    }

    /* Add to history */
    if (command_history_count < COMMAND_HISTORY_MAX) {
        command_history[command_history_count++] = strdup(cmd);
    } else {
        /* Shift and reuse oldest slot */
        free(command_history[0]);
        memmove(command_history, command_history + 1,
                sizeof(char*) * (COMMAND_HISTORY_MAX - 1));
        command_history[COMMAND_HISTORY_MAX - 1] = strdup(cmd);
    }
}

const char* command_history_get(int index) {
    if (index < 0 || index >= command_history_count) return NULL;
    return command_history[index];
}

int command_history_len(void) {
    return command_history_count;
}

void command_history_free(void) {
    for (int i = 0; i < command_history_count; i++) {
        free(command_history[i]);
        command_history[i] = NULL;
    }
    command_history_count = 0;
}

/* ======================== Command Lookup ======================== */

/* Find command definition (builtin or dynamic) */
static command_def_t* find_command(const char *name) {
    /* Check built-in commands */
    for (int i = 0; builtin_commands[i].name != NULL; i++) {
        if (strcmp(builtin_commands[i].name, name) == 0) {
            return &builtin_commands[i];
        }
    }

    /* Check dynamic (Lua-registered) commands */
    for (int i = 0; i < dynamic_command_count; i++) {
        if (strcmp(dynamic_commands[i].name, name) == 0) {
            return &dynamic_commands[i];
        }
    }

    return NULL;
}

/* Public API for command lookup (used by cmd_help) */
command_def_t* command_find(const char *name) {
    return find_command(name);
}

/* ======================== Command Parsing ======================== */

/* Parse command line into command name and arguments */
/* Count whitespace-separated arguments in an argument string. */
static int count_args(const char *args) {
    if (!args) return 0;
    int n = 0;
    const char *p = args;
    while (*p) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        n++;
        while (*p && !isspace((unsigned char)*p)) p++;
    }
    return n;
}

static int parse_command(const char *cmdline, char **cmd_name, char **args) {
    /* Skip leading ':' and whitespace */
    while (*cmdline && (*cmdline == ':' || isspace(*cmdline))) {
        cmdline++;
    }

    if (!*cmdline) return 0;  /* Empty command */

    /* Find command name (up to first space or end) */
    const char *cmd_end = cmdline;
    while (*cmd_end && !isspace(*cmd_end)) {
        cmd_end++;
    }

    size_t cmd_len = cmd_end - cmdline;
    *cmd_name = malloc(cmd_len + 1);
    if (!*cmd_name) return 0;

    memcpy(*cmd_name, cmdline, cmd_len);
    (*cmd_name)[cmd_len] = '\0';

    /* Skip whitespace after command */
    while (*cmd_end && isspace(*cmd_end)) {
        cmd_end++;
    }

    /* Rest is arguments */
    if (*cmd_end) {
        *args = strdup(cmd_end);
    } else {
        *args = NULL;
    }

    return 1;
}

/* Helper: check if string is all digits */
static int is_all_digits(const char *s) {
    if (!s || !*s) return 0;
    while (*s) {
        if (!isdigit((unsigned char)*s)) return 0;
        s++;
    }
    return 1;
}

/* Helper: check if command is a substitute pattern */
static int is_substitute_pattern(const char *cmd) {
    /* Match s/.../.../[g] pattern */
    if (!cmd) return 0;
    if (cmd[0] != 's' || cmd[1] != '/') return 0;
    return 1;
}

/* ======================== Command Execution ======================== */

int command_execute(editor_ctx_t *ctx, const char *cmdline) {
    char *cmd_name = NULL;
    char *args = NULL;

    if (!parse_command(cmdline, &cmd_name, &args)) {
        editor_set_status_msg(ctx, "");
        return 0;
    }

    /* Add to history, skipping a leading ':' if present. command_execute is
     * also called with bare command strings, where the ':' is absent. */
    command_history_add(cmdline[0] == ':' ? cmdline + 1 : cmdline);

    /* Special case: numeric command like :123 -> go to line 123 */
    if (is_all_digits(cmd_name)) {
        int result = cmd_goto(ctx, cmd_name);
        free(cmd_name);
        free(args);
        return result;
    }

    /* Special case: substitute pattern like :s/old/new/[g] */
    if (is_substitute_pattern(cmd_name)) {
        /* Reconstruct the full pattern (cmd_name + args) */
        char pattern[512];
        if (args && args[0]) {
            snprintf(pattern, sizeof(pattern), "%s %s", cmd_name, args);
        } else {
            snprintf(pattern, sizeof(pattern), "%s", cmd_name);
        }
        int result = cmd_substitute(ctx, pattern);
        free(cmd_name);
        free(args);
        return result;
    }

    /* Find command handler */
    command_def_t *cmd = find_command(cmd_name);
    if (!cmd) {
        editor_set_status_msg(ctx, "Unknown command: %s", cmd_name);
        free(cmd_name);
        free(args);
        return 0;
    }

    /* Validate argument count. */
    int arg_count = count_args(args);
    if (arg_count < cmd->min_args) {
        editor_set_status_msg(ctx, ":%s requires %d argument%s", cmd_name,
                              cmd->min_args, cmd->min_args == 1 ? "" : "s");
        free(cmd_name);
        free(args);
        return 0;
    }
    if (cmd->max_args >= 0 && arg_count > cmd->max_args) {
        editor_set_status_msg(ctx, ":%s takes at most %d argument%s", cmd_name,
                              cmd->max_args, cmd->max_args == 1 ? "" : "s");
        free(cmd_name);
        free(args);
        return 0;
    }

    /* Store command name for Lua handlers (they need to know which command was called) */
    if (ctx_L(ctx)) {
        lua_State *L = ctx_L(ctx);
        lua_pushstring(L, cmd_name);
        lua_setglobal(L, "_loki_ex_command_executing");
    }

    /* Execute command */
    int result = cmd->handler(ctx, args);

    /* Clear the command name */
    if (ctx_L(ctx)) {
        lua_State *L = ctx_L(ctx);
        lua_pushnil(L);
        lua_setglobal(L, "_loki_ex_command_executing");
    }

    free(cmd_name);
    free(args);
    return result;
}

/* ======================== Command Mode Input Handling ======================== */

void command_mode_handle_key(editor_ctx_t *ctx, int fd, int key) {
    (void)fd;  /* fd parameter for future use */

    switch (key) {
        case ESC:
            /* Abandoned command line: drop the ':' prompt too. */
            editor_set_status_msg(ctx, "");
            command_mode_exit(ctx);
            break;

        case ENTER:
            /* Execute command */
            if (ctx->view.cmd_length > 1) {  /* More than just ':' */
                command_execute(ctx, ctx->view.cmd_buffer);
            }
            /* Get current context after command execution - buffer may have switched */
            command_mode_exit(buffer_get_current());
            break;

        case BACKSPACE:
        case CTRL_H:
        case DEL_KEY:
            if (ctx->view.cmd_cursor_pos > 1) {  /* Can't delete ':' */
                /* Remove character before cursor */
                memmove(ctx->view.cmd_buffer + ctx->view.cmd_cursor_pos - 1,
                        ctx->view.cmd_buffer + ctx->view.cmd_cursor_pos,
                        ctx->view.cmd_length - ctx->view.cmd_cursor_pos + 1);
                ctx->view.cmd_cursor_pos--;
                ctx->view.cmd_length--;
                editor_set_status_msg(ctx, "%s", ctx->view.cmd_buffer);
            } else {
                /* Backspace on empty command exits command mode */
                editor_set_status_msg(ctx, "");
                command_mode_exit(ctx);
            }
            break;

        case ARROW_LEFT:
            if (ctx->view.cmd_cursor_pos > 1) {
                ctx->view.cmd_cursor_pos--;
            }
            break;

        case ARROW_RIGHT:
            if (ctx->view.cmd_cursor_pos < ctx->view.cmd_length) {
                ctx->view.cmd_cursor_pos++;
            }
            break;

        case ARROW_UP:
            /* Previous command in history */
            if (ctx->view.cmd_history_index > 0) {
                ctx->view.cmd_history_index--;
                const char *hist = command_history_get(ctx->view.cmd_history_index);
                if (hist) {
                    ctx->view.cmd_buffer[0] = ':';
                    strncpy(ctx->view.cmd_buffer + 1, hist,
                            sizeof(ctx->view.cmd_buffer) - 2);
                    ctx->view.cmd_buffer[sizeof(ctx->view.cmd_buffer) - 1] = '\0';
                    ctx->view.cmd_length = strlen(ctx->view.cmd_buffer);
                    ctx->view.cmd_cursor_pos = ctx->view.cmd_length;
                    editor_set_status_msg(ctx, "%s", ctx->view.cmd_buffer);
                }
            }
            break;

        case ARROW_DOWN:
            /* Next command in history */
            if (ctx->view.cmd_history_index < command_history_count - 1) {
                ctx->view.cmd_history_index++;
                const char *hist = command_history_get(ctx->view.cmd_history_index);
                if (hist) {
                    ctx->view.cmd_buffer[0] = ':';
                    strncpy(ctx->view.cmd_buffer + 1, hist,
                            sizeof(ctx->view.cmd_buffer) - 2);
                    ctx->view.cmd_buffer[sizeof(ctx->view.cmd_buffer) - 1] = '\0';
                    ctx->view.cmd_length = strlen(ctx->view.cmd_buffer);
                    ctx->view.cmd_cursor_pos = ctx->view.cmd_length;
                    editor_set_status_msg(ctx, "%s", ctx->view.cmd_buffer);
                }
            } else {
                /* At end of history, clear command */
                ctx->view.cmd_buffer[0] = ':';
                ctx->view.cmd_buffer[1] = '\0';
                ctx->view.cmd_length = 1;
                ctx->view.cmd_cursor_pos = 1;
                ctx->view.cmd_history_index = command_history_count;
                editor_set_status_msg(ctx, ":");
            }
            break;

        case CTRL_U:
            /* Clear command line */
            ctx->view.cmd_buffer[0] = ':';
            ctx->view.cmd_buffer[1] = '\0';
            ctx->view.cmd_length = 1;
            ctx->view.cmd_cursor_pos = 1;
            editor_set_status_msg(ctx, ":");
            break;

        default:
            /* Regular character input */
            if (isprint(key) && ctx->view.cmd_length < COMMAND_BUFFER_SIZE - 1) {
                /* Insert character at cursor */
                memmove(ctx->view.cmd_buffer + ctx->view.cmd_cursor_pos + 1,
                        ctx->view.cmd_buffer + ctx->view.cmd_cursor_pos,
                        ctx->view.cmd_length - ctx->view.cmd_cursor_pos + 1);
                ctx->view.cmd_buffer[ctx->view.cmd_cursor_pos] = key;
                ctx->view.cmd_cursor_pos++;
                ctx->view.cmd_length++;
                editor_set_status_msg(ctx, "%s", ctx->view.cmd_buffer);
            }
            break;
    }
}

/* ======================== Dynamic Command Registration (for Lua) ======================== */

int command_register(const char *name, command_handler_t handler,
                     const char *help, int min_args, int max_args) {
    if (dynamic_command_count >= MAX_DYNAMIC_COMMANDS) {
        return 0;  /* Registry full */
    }

    /* Check if command already exists */
    if (find_command(name)) {
        return 0;  /* Can't override built-in or existing command */
    }

    /* Register new command */
    dynamic_commands[dynamic_command_count].name = strdup(name);
    dynamic_commands[dynamic_command_count].handler = handler;
    dynamic_commands[dynamic_command_count].help = strdup(help);
    dynamic_commands[dynamic_command_count].min_args = min_args;
    dynamic_commands[dynamic_command_count].max_args = max_args;
    dynamic_command_count++;

    return 1;
}

void command_unregister_all_dynamic(void) {
    for (int i = 0; i < dynamic_command_count; i++) {
        free((char*)dynamic_commands[i].name);
        free((char*)dynamic_commands[i].help);
    }
    dynamic_command_count = 0;
}
