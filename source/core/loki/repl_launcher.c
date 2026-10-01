/**
 * @file repl_launcher.c
 * @brief Shared REPL launcher implementation.
 *
 * Provides common REPL startup logic for all music languages.
 */

#include "repl_launcher.h"
#include "psnd.h"
#include "loki/core.h"
#include "syntax.h"
#include "soundfont.h"
#include "loki/lua.h"

#include <lua.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

/* ============================================================================
 * CLI Argument Parsing
 * ============================================================================ */

typedef struct {
    SharedReplArgs args;
    const char *input_file;
    int show_help;
    int list_ports;
} ParsedArgs;

static void parse_repl_args(ParsedArgs *parsed, int argc, char **argv) {
    memset(parsed, 0, sizeof(*parsed));
    parsed->args.port_index = -1;

    /* Skip argv[0] which is the language name (e.g., "joy", "tr7") */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            parsed->show_help = 1;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            parsed->args.verbose = 1;
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) {
            parsed->list_ports = 1;
        } else if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) &&
                   i + 1 < argc) {
            parsed->args.port_index = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--virtual") == 0 && i + 1 < argc) {
            parsed->args.virtual_name = argv[++i];
        } else if ((strcmp(argv[i], "-sf") == 0 || strcmp(argv[i], "--soundfont") == 0) &&
                   i + 1 < argc) {
            parsed->args.soundfont_path = argv[++i];
        } else if (argv[i][0] != '-' && parsed->input_file == NULL) {
            parsed->input_file = argv[i];
        }
    }
    parsed->args.soundfont_path = psnd_soundfont_resolve(parsed->args.soundfont_path,
        parsed->args.port_index >= 0 || parsed->args.virtual_name || parsed->list_ports ||
        parsed->show_help);
}

static void parse_play_args(ParsedArgs *parsed, int argc, char **argv) {
    memset(parsed, 0, sizeof(*parsed));
    parsed->args.port_index = -1;

    /* argv[0] may be the filename for play mode */
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            parsed->args.verbose = 1;
        } else if ((strcmp(argv[i], "-sf") == 0 || strcmp(argv[i], "--soundfont") == 0) &&
                   i + 1 < argc) {
            parsed->args.soundfont_path = argv[++i];
        } else if (argv[i][0] != '-' && parsed->input_file == NULL) {
            parsed->input_file = argv[i];
        }
    }
    parsed->args.soundfont_path = psnd_soundfont_resolve(parsed->args.soundfont_path, 0);
}

/* ============================================================================
 * Syntax Highlighting Setup
 * ============================================================================ */

static void setup_syntax_context(editor_ctx_t *syntax_ctx, const char *file_ext) {
    char dummy_filename[32];
    snprintf(dummy_filename, sizeof(dummy_filename), "input%s", file_ext);

    editor_ctx_init(syntax_ctx);
    syntax_init_default_colors(syntax_ctx);
    syntax_select_for_filename(syntax_ctx, dummy_filename);

    /* Load Lua and themes for consistent highlighting */
    LuaHost *lua_host = lua_host_create();
    if (lua_host) {
        syntax_ctx->lua_host = lua_host;
        struct loki_lua_opts lua_opts = {
            .bind_editor = 1,
            .load_config = 1,
            .reporter = NULL
        };
        lua_host->L = loki_lua_bootstrap(syntax_ctx, &lua_opts);
    }
}

static void cleanup_syntax_context(editor_ctx_t *syntax_ctx) {
    if (syntax_ctx->lua_host) {
        lua_host_free(syntax_ctx->lua_host);
        syntax_ctx->lua_host = NULL;
    }
}

/* Get program name from callbacks, defaulting to PSND_NAME */
static const char *get_prog_name(const SharedReplCallbacks *callbacks) {
    return callbacks->prog_name ? callbacks->prog_name : PSND_NAME;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

int shared_lang_repl_main(const SharedReplCallbacks *callbacks, int argc, char **argv) {
    if (!callbacks || !callbacks->name || !callbacks->init || !callbacks->cleanup) {
        fprintf(stderr, "Error: Invalid REPL callbacks\n");
        return 1;
    }

    /* Parse arguments */
    ParsedArgs parsed;
    parse_repl_args(&parsed, argc, argv);

    /* Handle --help */
    if (parsed.show_help) {
        const char *prog = get_prog_name(callbacks);
        if (callbacks->print_usage) {
            callbacks->print_usage(prog);
        } else {
            printf("Usage: %s %s [options] [file]\n", prog, callbacks->name);
        }
        return 0;
    }

    /* Handle --list */
    if (parsed.list_ports) {
        if (callbacks->list_ports) {
            callbacks->list_ports();
        } else {
            fprintf(stderr, "Error: %s does not support port listing\n", callbacks->name);
        }
        return 0;
    }

    /* Initialize language (includes MIDI/audio setup) */
    void *lang_ctx = callbacks->init(&parsed.args);
    if (!lang_ctx) {
        fprintf(stderr, "Error: Failed to initialize %s\n", callbacks->name);
        return 1;
    }

    int result = 0;

    if (parsed.input_file) {
        /* File mode */
        if (parsed.args.verbose) {
            printf("Executing: %s\n", parsed.input_file);
        }

        if (callbacks->exec_file) {
            result = callbacks->exec_file(lang_ctx, parsed.input_file, parsed.args.verbose);
        } else {
            fprintf(stderr, "Error: %s does not support file execution\n", callbacks->name);
            result = 1;
        }
    } else {
        /* REPL mode */
        if (callbacks->repl_loop) {
            editor_ctx_t syntax_ctx;
            setup_syntax_context(&syntax_ctx,
                                 callbacks->file_ext ? callbacks->file_ext : ".txt");

            callbacks->repl_loop(lang_ctx, &syntax_ctx);

            cleanup_syntax_context(&syntax_ctx);
        } else {
            fprintf(stderr, "Error: %s does not support REPL mode\n", callbacks->name);
            result = 1;
        }
    }

    /* Cleanup */
    callbacks->cleanup(lang_ctx);

    return result;
}

int shared_lang_play_main(const SharedReplCallbacks *callbacks, int argc, char **argv) {
    if (!callbacks || !callbacks->name || !callbacks->init ||
        !callbacks->cleanup || !callbacks->exec_file) {
        fprintf(stderr, "Error: Invalid REPL callbacks\n");
        return 1;
    }

    /* Parse arguments */
    ParsedArgs parsed;
    parse_play_args(&parsed, argc, argv);

    if (!parsed.input_file) {
        fprintf(stderr, "Usage: %s play [-v] [-sf soundfont.sf2] <file%s>\n",
                get_prog_name(callbacks),
                callbacks->file_ext ? callbacks->file_ext : "");
        return 1;
    }

    /* Initialize language */
    void *lang_ctx = callbacks->init(&parsed.args);
    if (!lang_ctx) {
        fprintf(stderr, "Error: Failed to initialize %s\n", callbacks->name);
        return 1;
    }

    /* Execute file */
    if (parsed.args.verbose) {
        printf("Executing: %s\n", parsed.input_file);
    }

    int result = callbacks->exec_file(lang_ctx, parsed.input_file, parsed.args.verbose);

    /* Cleanup */
    callbacks->cleanup(lang_ctx);

    return result;
}
