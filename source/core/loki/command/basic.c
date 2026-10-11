/* basic.c - Basic editor commands (:q, :wq, :help, :set, :stop)
 *
 * Core commands for quitting, help, settings, and playback control.
 */

#include "command_impl.h"
#include "../live_loop.h"
#include "../lang_bridge.h"

/* :q, :quit - Quit editor */
int cmd_quit(editor_ctx_t *ctx, const char *args) {
    (void)args;

    if (ctx->model.dirty) {
        editor_set_status_msg(ctx, "Unsaved changes! Use :q! to force quit");
        return 0;
    }

    /* Exit program */
    exit(0);
}

/* :q!, :quit! - Force quit without saving */
int cmd_force_quit(editor_ctx_t *ctx, const char *args) {
    (void)ctx;
    (void)args;

    /* Exit without checking dirty flag */
    exit(0);
}

/* :wq, :x - Write and quit */
int cmd_write_quit(editor_ctx_t *ctx, const char *args) {
    /* Save first */
    if (!cmd_write(ctx, args)) {
        return 0;
    }

    /* Then quit */
    exit(0);
}

/* :help, :h - Show help */
int cmd_help(editor_ctx_t *ctx, const char *args) {
    if (!args || !args[0]) {
        /* Show general help */
        editor_set_status_msg(ctx,
            "Commands: :w :q :wq :set :e :help <cmd> | Ctrl-F=find Ctrl-S=save");
        return 1;
    }

    /* Show help for specific command */
    command_def_t *cmd = command_find(args);
    if (cmd) {
        editor_set_status_msg(ctx, ":%s - %s", cmd->name, cmd->help);
        return 1;
    } else {
        editor_set_status_msg(ctx, "Unknown command: %s", args);
        return 0;
    }
}

/* :set - Set editor options */
int cmd_set(editor_ctx_t *ctx, const char *args) {
    if (!args || !args[0]) {
        /* Show current settings */
        editor_set_status_msg(ctx, "Options: wrap");
        return 1;
    }

    /* Parse "set option" or "set option=value" */
    char option[64] = {0};
    char value[64] = {0};

    if (sscanf(args, "%63s = %63s", option, value) == 2 ||
        sscanf(args, "%63s=%63s", option, value) == 2) {
        /* No option currently takes a value; report failure rather than
         * claiming success for a setting that was never applied. */
        editor_set_status_msg(ctx, "Unknown option: %s", option);
        return 0;
    } else if (sscanf(args, "%63s", option) == 1) {
        /* Toggle boolean option or show value */
        if (strcmp(option, "wrap") == 0) {
            ctx->view.word_wrap = !ctx->view.word_wrap;
            editor_set_status_msg(ctx, "Word wrap: %s",
                                 ctx->view.word_wrap ? "on" : "off");
            return 1;
        } else {
            editor_set_status_msg(ctx, "Unknown option: %s", option);
            return 0;
        }
    }

    return 0;
}

/* :stop - Stop all playback and live loops */
int cmd_stop(editor_ctx_t *ctx, const char *args) {
    (void)args;

    /* Stop metronome */
    metronome_stop();

    /* Stop all live loops */
    live_loop_shutdown();

    /* Stop all language playback */
    loki_lang_stop_all(ctx);

    editor_set_status_msg(ctx, "Stopped");
    return 1;
}

/* :play - Play entire buffer (same as Ctrl-P) */
int cmd_play(editor_ctx_t *ctx, const char *args) {
    (void)args;

    if (ctx->model.numrows == 0) {
        editor_set_status_msg(ctx, "Empty buffer");
        return 0;
    }

    const LokiLangOps *lang = loki_lang_for_file(ctx->model.filename);
    if (!lang) {
        editor_set_status_msg(ctx, "No language support for this file type");
        return 0;
    }

    int ret = loki_lang_eval_buffer(ctx);
    if (ret == 0) {
        editor_set_status_msg(ctx, "%s: playing", lang->name);
        return 1;
    } else {
        const char *err = loki_lang_get_error(ctx);
        editor_set_status_msg(ctx, "%s error: %s", lang->name,
            err ? err : "eval failed");
        return 0;
    }
}

/* :eval [code] - Evaluate code or current line (same as Ctrl-E) */
int cmd_eval(editor_ctx_t *ctx, const char *args) {
    const LokiLangOps *lang = loki_lang_for_file(ctx->model.filename);
    if (!lang) {
        editor_set_status_msg(ctx, "No language support for this file type");
        return 0;
    }

    const char *code = args;
    char *line_copy = NULL;

    /* If no args, use current line */
    if (!args || !args[0]) {
        if (ctx->view.cy >= ctx->model.numrows) {
            editor_set_status_msg(ctx, "No code to evaluate");
            return 0;
        }
        t_erow *row = &ctx->model.row[ctx->view.cy];
        if (row->size == 0) {
            editor_set_status_msg(ctx, "Empty line");
            return 0;
        }
        line_copy = malloc(row->size + 1);
        if (!line_copy) {
            editor_set_status_msg(ctx, "Out of memory");
            return 0;
        }
        memcpy(line_copy, row->chars, row->size);
        line_copy[row->size] = '\0';
        code = line_copy;
    }

    int ret = loki_lang_eval(ctx, code);
    free(line_copy);

    if (ret == 0) {
        editor_set_status_msg(ctx, "%s: evaluated", lang->name);
        return 1;
    } else {
        const char *err = loki_lang_get_error(ctx);
        editor_set_status_msg(ctx, "%s error: %s", lang->name,
            err ? err : "eval failed");
        return 0;
    }
}
