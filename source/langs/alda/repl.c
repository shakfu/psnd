/**
 * @file repl.c
 * @brief Alda REPL - Interactive music composition terminal with syntax highlighting.
 */

#include "repl.h"
#include "psnd.h"
#include "loki/core.h"
#include "loki/internal.h"
#include "loki/syntax.h"
#include "loki/lua.h"
#include "loki/soundfont.h"
#include "loki/repl_helpers.h"
#include "shared/repl_commands.h"

/* Alda library headers */
#include "alda/alda.h"
#include "alda/context.h"
#include "alda/midi_backend.h"
#include "alda/tsf_backend.h"
#include "alda/csound_backend.h"
#include "alda/scheduler.h"
#include "alda/interpreter.h"
#include "alda/async.h"
#include "alda/instruments.h"

/* Shared context for REPL-owned state */
#include "shared/context.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define usleep(us) Sleep((us) / 1000)
#define isatty _isatty
#define STDIN_FILENO 0
#else
#include <unistd.h>
#include <getopt.h>
#endif

/* ============================================================================
 * Alda Usage and Help
 * ============================================================================ */

static void print_repl_usage(const char *prog) {
    printf("Usage: %s [options] [file.alda]\n", prog);
    printf("\n");
    printf("Alda music language interpreter with MIDI output.\n");
    printf("If no file is provided, starts an interactive REPL.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help        Show this help message\n");
    printf("  -v, --verbose     Enable verbose output\n");
    printf("  -l, --list        List available MIDI ports\n");
    printf("  -p, --port N      Use MIDI port N (0-based index)\n");
    printf("  -o, --output NAME Use MIDI port matching NAME\n");
    printf("  --virtual NAME    Create virtual MIDI port with NAME\n");
    printf("  -s, --sequential  Use sequential playback mode\n");
    printf("\n");
    printf("Built-in Synth Options:\n");
    printf("  -sf, --soundfont PATH  Use built-in synth with soundfont (.sf2)\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s                      Start interactive REPL\n", prog);
    printf("  %s song.alda            Play an Alda file\n", prog);
    printf("  %s -sf gm.sf2           REPL with built-in synth\n", prog);
    printf("  %s -sf gm.sf2 song.alda Play with built-in synth\n", prog);
    printf("\n");
}

static void print_repl_help(void) {
    shared_print_command_help();

    printf("Alda-specific Commands:\n");
    printf("  :sequential       Wait for each input to complete\n");
    printf("  :concurrent       Enable polyphonic playback (default)\n");
    printf("\n");
    printf("Alda Syntax Examples:\n");
    printf("  piano:            Select piano instrument\n");
    printf("  c d e f g         Play notes C D E F G\n");
    printf("  c4 d8 e8 f4       Quarter, eighths, quarter\n");
    printf("  c/e/g             Play C major chord\n");
    printf("  (tempo 140)       Set tempo to 140 BPM\n");
    printf("  o5 c d e          Octave 5, then notes\n");
    printf("\n");
}

/* ============================================================================
 * REPL Loop
 * ============================================================================ */

/* Note: Using repl_repl_starts_with() from loki/repl_helpers.h */

/* Process an Alda REPL command. Returns: 0=continue, 1=quit, 2=interpret as Alda */
static int alda_process_command(AldaContext *ctx, const char *input) {
    /* Try shared commands first */
    int result = shared_process_command(ctx->shared, input, alda_async_stop);
    if (result == REPL_CMD_QUIT) {
        return 1; /* quit */
    }
    if (result == REPL_CMD_HANDLED) {
        /* Sync builtin_synth_enabled flag from shared context */
        if (ctx->shared) {
            ctx->builtin_synth_enabled = ctx->shared->builtin_synth_enabled;
            ctx->csound_enabled = ctx->shared->csound_enabled;
        }
        return 0;
    }

    /* Handle Alda-specific commands */
    const char *cmd = input;
    if (cmd[0] == ':')
        cmd++;

    /* Help - add Alda-specific help */
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0 || strcmp(cmd, "?") == 0) {
        print_repl_help();
        return 0;
    }

    /* :play file.alda - interpret and play an Alda file */
    if (repl_starts_with(cmd, "play ")) {
        const char* path = cmd + 5;
        while (*path == ' ') path++;
        if (*path) {
            printf("Playing %s...\n", path);
            alda_events_clear(ctx);
            int parse_result = alda_interpret_file(ctx, path);
            if (parse_result < 0) {
                printf("Failed to parse file: %s\n", path);
            } else if (ctx->event_count > 0) {
                alda_events_play_async(ctx);
            } else {
                printf("No events to play\n");
            }
        } else {
            printf("Usage: :play PATH\n");
        }
        return 0;
    }

    if (strcmp(cmd, "concurrent") == 0) {
        alda_async_set_concurrent(1);
        printf("Concurrent mode enabled (polyphony)\n");
        return 0;
    }

    if (strcmp(cmd, "sequential") == 0) {
        alda_async_set_concurrent(0);
        printf("Sequential mode enabled\n");
        return 0;
    }

    return 2; /* interpret as Alda */
}

/* Non-interactive Alda REPL loop for piped input */
static void alda_repl_loop_pipe(AldaContext *ctx) {
    char line[MAX_INPUT_LENGTH];

    while (fgets(line, sizeof(line), stdin) != NULL) {
        /* Strip trailing newline */
        size_t len = repl_strip_newlines(line);
        if (len == 0) continue;

        int result = alda_process_command(ctx, line);
        if (result == 1) break;      /* quit */
        if (result == 0) continue;   /* command handled */

        /* Interpret as Alda */
        alda_events_clear(ctx);
        int parse_result = alda_interpret_string(ctx, line, "<pipe>");
        if (parse_result < 0) continue;

        if (ctx->event_count > 0) {
            if (ctx->verbose_mode) {
                printf("Playing %d events...\n", ctx->event_count);
            }
            alda_events_play_async(ctx);
        }
    }
}

/* Tab completion callback for Alda REPL - completes instrument names */
static char **alda_completion_callback(const char *prefix, int *count, void *user_data) {
    (void)user_data;  /* Alda completion doesn't need context */
    return alda_instrument_get_completions(prefix, count, REPL_COMPLETIONS_MAX);
}

static void repl_loop(AldaContext *ctx, editor_ctx_t *syntax_ctx) {
    ReplLineEditor ed;
    char *input;
    char history_path[512] = {0};

    /* Use non-interactive mode for piped input */
    if (!isatty(STDIN_FILENO)) {
        alda_repl_loop_pipe(ctx);
        return;
    }

#ifdef LOKI_USE_LINENOISE
    repl_editor_init_with_language(&ed, REPL_LANG_ALDA);
#else
    repl_editor_init(&ed);
#endif

    /* Set up tab completion for instrument names */
    repl_set_completion(&ed, alda_completion_callback, NULL);

    /* Build history file path and load history */
    if (repl_get_history_path("alda", history_path, sizeof(history_path))) {
        repl_history_load(&ed, history_path);
    }

    printf("Alda REPL %s (type :h for help, :q to quit)\n", PSND_VERSION);
    if (!alda_async_get_concurrent()) {
        printf("Mode: sequential\n");
    }

    /* Enable raw mode for syntax-highlighted input */
    repl_enable_raw_mode();

    while (1) {
        input = repl_readline(syntax_ctx, &ed, "alda> ");

        if (input == NULL) {
            /* EOF - exit cleanly */
            break;
        }

        if (input[0] == '\0') {
            continue;
        }

        repl_add_history(&ed, input);

        /* Process command */
        int result = alda_process_command(ctx, input);
        if (result == 1) break;      /* quit */
        if (result == 0) {
            /* Command handled - poll Link callbacks */
            shared_repl_link_check();
            continue;
        }

        /* Alda interpretation */
        alda_events_clear(ctx);

        int parse_result = alda_interpret_string(ctx, input, "<repl>");
        if (parse_result < 0) {
            shared_repl_link_check();
            continue;
        }

        if (ctx->event_count > 0) {
            if (ctx->verbose_mode) {
                printf("Playing %d events...\n", ctx->event_count);
            }
            alda_events_play_async(ctx);
        }

        /* Poll Link callbacks after evaluation */
        shared_repl_link_check();
    }

    /* Disable raw mode before exit */
    repl_disable_raw_mode();

    /* Save history */
    if (history_path[0]) {
        repl_history_save(&ed, history_path);
    }

    repl_editor_cleanup(&ed);
}

/* ============================================================================
 * File Playback (headless)
 * ============================================================================ */

/* Helper function to check for .csd extension */
static int is_csd_file(const char *path) {
    if (!path) return 0;
    size_t len = strlen(path);
    if (len < 4) return 0;
    return strcmp(path + len - 4, ".csd") == 0;
}

int alda_play_main(int argc, char **argv) {
    int verbose = 0;
    const char *soundfont_path = NULL;
    const char *input_file = NULL;

    /* Simple argument parsing */
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = 1;
        } else if ((strcmp(argv[i], "-sf") == 0 || strcmp(argv[i], "--soundfont") == 0) &&
                   i + 1 < argc) {
            soundfont_path = argv[++i];
        } else if (argv[i][0] != '-' && input_file == NULL) {
            input_file = argv[i];
        }
    }

    if (!input_file) {
        fprintf(stderr, "Usage: " PSND_NAME " play [-v] [-sf soundfont.sf2] <file.alda|file.joy|file.csd>\n");
        return 1;
    }
    soundfont_path = psnd_soundfont_resolve(soundfont_path, is_csd_file(input_file));

    /* Handle .csd files with Csound backend */
    if (is_csd_file(input_file)) {
        if (soundfont_path) {
            fprintf(stderr, "Warning: -sf option ignored for .csd files\n");
        }
        int result = alda_csound_play_file(input_file, verbose);
        if (result != 0) {
            const char *err = alda_csound_get_error();
            fprintf(stderr, "Error: %s\n", err ? err : "Failed to play CSD file");
            return 1;
        }
        return 0;
    }

    /* Initialize */
    AldaContext ctx;
    alda_context_init(&ctx);
    ctx.verbose_mode = verbose;

    /* Create REPL-owned SharedContext for audio/MIDI/Link */
    SharedContext shared;
    if (shared_context_init(&shared) != 0) {
        fprintf(stderr, "Error: Failed to initialize shared context\n");
        alda_context_cleanup(&ctx);
        return 1;
    }
    ctx.shared = &shared;

    if (alda_tsf_init() != 0) {
        fprintf(stderr, "Warning: Failed to initialize built-in synth\n");
    }

    alda_midi_init_observer(&ctx);

    /* Setup output */
    if (soundfont_path) {
        if (alda_tsf_load_soundfont(soundfont_path) != 0) {
            fprintf(stderr, "Error: Failed to load soundfont: %s\n", soundfont_path);
            alda_tsf_cleanup();
            alda_midi_cleanup(&ctx);
            alda_context_cleanup(&ctx);
            return 1;
        }
        if (alda_tsf_enable() != 0) {
            fprintf(stderr, "Error: Failed to enable built-in synth\n");
            alda_tsf_cleanup();
            alda_midi_cleanup(&ctx);
            alda_context_cleanup(&ctx);
            return 1;
        }
        ctx.builtin_synth_enabled = 1;
        ctx.shared->builtin_synth_enabled = 1;
        if (verbose) {
            printf("Using built-in synth: %s\n", soundfont_path);
        }
    } else {
        if (alda_midi_open_auto(&ctx, PSND_MIDI_PORT_NAME) != 0) {
            fprintf(stderr, "Warning: No MIDI output available\n");
            fprintf(stderr, "Hint: Use -sf <soundfont.sf2> for built-in synth\n");
        }
    }

    /* Interpret file */
    if (verbose) {
        printf("Playing: %s\n", input_file);
    }

    int result = alda_interpret_file(&ctx, input_file);
    if (result < 0) {
        fprintf(stderr, "Error: Failed to interpret file\n");
        alda_tsf_cleanup();
        alda_midi_cleanup(&ctx);
        alda_context_cleanup(&ctx);
        return 1;
    }

    if (verbose) {
        printf("Scheduled %d events\n", ctx.event_count);
    }

    /* Play (blocking) */
    result = alda_events_play(&ctx);

    /* Cleanup */
    alda_async_cleanup();
    alda_tsf_cleanup();
    alda_midi_cleanup(&ctx);
    alda_context_cleanup(&ctx);
    shared_context_cleanup(&shared);

    return result < 0 ? 1 : 0;
}

/* ============================================================================
 * REPL Main Entry Point
 * ============================================================================ */

int alda_repl_main(int argc, char **argv) {
    int verbose = 0;
    int list_ports = 0;
    int port_index = -1;
    const char *port_name = NULL;
    const char *virtual_name = NULL;
    int sequential = 0;
    const char *input_file = NULL;
    const char *soundfont_path = NULL;

#ifdef _WIN32
    /* Simple argument parsing for Windows */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_repl_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--verbose") == 0 || strcmp(argv[i], "-v") == 0) {
            verbose = 1;
        } else if (strcmp(argv[i], "--list") == 0 || strcmp(argv[i], "-l") == 0) {
            list_ports = 1;
        } else if (strcmp(argv[i], "--sequential") == 0 || strcmp(argv[i], "-s") == 0) {
            sequential = 1;
        } else if ((strcmp(argv[i], "--port") == 0 || strcmp(argv[i], "-p") == 0) &&
                   i + 1 < argc) {
            port_index = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "--output") == 0 || strcmp(argv[i], "-o") == 0) &&
                   i + 1 < argc) {
            port_name = argv[++i];
        } else if (strcmp(argv[i], "--virtual") == 0 && i + 1 < argc) {
            virtual_name = argv[++i];
        } else if ((strcmp(argv[i], "--soundfont") == 0 || strcmp(argv[i], "-sf") == 0) &&
                   i + 1 < argc) {
            soundfont_path = argv[++i];
        } else if (argv[i][0] != '-') {
            input_file = argv[i];
        }
    }
#else
    /* Pre-process -sf */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-sf") == 0 && i + 1 < argc) {
            soundfont_path = argv[i + 1];
            argv[i] = "";
            argv[i + 1] = "";
            i++;
        }
    }

    static struct option long_options[] = {{"help", no_argument, 0, 'h'},
                                           {"verbose", no_argument, 0, 'v'},
                                           {"list", no_argument, 0, 'l'},
                                           {"port", required_argument, 0, 'p'},
                                           {"output", required_argument, 0, 'o'},
                                           {"virtual", required_argument, 0, 'V'},
                                           {"sequential", no_argument, 0, 's'},
                                           {"soundfont", required_argument, 0, 'F'},
                                           {0, 0, 0, 0}};

    int opt;
    int option_index = 0;
    while ((opt = getopt_long(argc, argv, "hvlsp:o:", long_options, &option_index)) != -1) {
        switch (opt) {
        case 'h':
            print_repl_usage(argv[0]);
            return 0;
        case 'v':
            verbose = 1;
            break;
        case 'l':
            list_ports = 1;
            break;
        case 'p':
            port_index = atoi(optarg);
            break;
        case 'o':
            port_name = optarg;
            break;
        case 'V':
            virtual_name = optarg;
            break;
        case 's':
            sequential = 1;
            break;
        case 'F':
            soundfont_path = optarg;
            break;
        default:
            print_repl_usage(argv[0]);
            return 1;
        }
    }

    /* Get input file */
    for (int i = optind; i < argc; i++) {
        if (argv[i][0] != '\0') {
            input_file = argv[i];
            break;
        }
    }
#endif
    soundfont_path = psnd_soundfont_resolve(soundfont_path,
        port_index >= 0 || port_name || virtual_name || list_ports);

    /* Initialize context */
    AldaContext ctx;
    alda_context_init(&ctx);
    ctx.verbose_mode = verbose;

    /* Create REPL-owned SharedContext for audio/MIDI/Link */
    SharedContext shared;
    if (shared_context_init(&shared) != 0) {
        fprintf(stderr, "Error: Failed to initialize shared context\n");
        alda_context_cleanup(&ctx);
        return 1;
    }
    ctx.shared = &shared;

    if (alda_tsf_init() != 0) {
        fprintf(stderr, "Warning: Failed to initialize built-in synth\n");
    }

    alda_midi_init_observer(&ctx);

    /* Handle --list */
    if (list_ports) {
        alda_midi_list_ports(&ctx);
        alda_tsf_cleanup();
        alda_midi_cleanup(&ctx);
        alda_context_cleanup(&ctx);
        shared_context_cleanup(&shared);
        return 0;
    }

    /* Setup output */
    if (soundfont_path) {
        if (alda_tsf_load_soundfont(soundfont_path) != 0) {
            fprintf(stderr, "Error: Failed to load soundfont: %s\n", soundfont_path);
            alda_tsf_cleanup();
            alda_midi_cleanup(&ctx);
            alda_context_cleanup(&ctx);
            shared_context_cleanup(&shared);
            return 1;
        }
        if (alda_tsf_enable() != 0) {
            fprintf(stderr, "Error: Failed to enable built-in synth\n");
            alda_tsf_cleanup();
            alda_midi_cleanup(&ctx);
            alda_context_cleanup(&ctx);
            shared_context_cleanup(&shared);
            return 1;
        }
        ctx.builtin_synth_enabled = 1;
        ctx.shared->builtin_synth_enabled = 1;
        if (verbose) {
            printf("Using built-in synth: %s\n", soundfont_path);
        }
    } else {
        int midi_opened = 0;

        if (virtual_name) {
            if (alda_midi_open_virtual(&ctx, virtual_name) == 0) {
                midi_opened = 1;
                if (verbose) {
                    printf("Created virtual MIDI output: %s\n", virtual_name);
                }
            }
        } else if (port_name) {
            if (alda_midi_open_by_name(&ctx, port_name) == 0) {
                midi_opened = 1;
            }
        } else if (port_index >= 0) {
            if (alda_midi_open_port(&ctx, port_index) == 0) {
                midi_opened = 1;
            }
        } else {
            if (alda_midi_open_auto(&ctx, PSND_MIDI_PORT_NAME) == 0) {
                midi_opened = 1;
            }
        }

        if (!midi_opened) {
            fprintf(stderr, "Warning: No MIDI output available\n");
            fprintf(stderr, "Hint: Use -sf <soundfont.sf2> for built-in synth\n");
        }
    }

    /* Set playback mode */
    if (!sequential) {
        alda_async_set_concurrent(1);
    }
    if (verbose) {
        printf("Playback mode: %s\n", sequential ? "sequential" : "concurrent");
    }

    int result = 0;

    if (input_file) {
        /* File mode */
        if (verbose) {
            printf("Playing: %s\n", input_file);
        }

        result = alda_interpret_file(&ctx, input_file);
        if (result < 0) {
            fprintf(stderr, "Error: Failed to interpret file\n");
            alda_midi_cleanup(&ctx);
            alda_context_cleanup(&ctx);
            shared_context_cleanup(&shared);
            return 1;
        }

        if (verbose) {
            printf("Scheduled %d events\n", ctx.event_count);
        }

        result = alda_events_play(&ctx);
    } else {
        /* REPL mode - initialize syntax highlighting with theme support */
        editor_ctx_t syntax_ctx;
        editor_ctx_init(&syntax_ctx);
        syntax_init_default_colors(&syntax_ctx);
        syntax_select_for_filename(&syntax_ctx, "input.alda");

        /* Load Lua and themes for consistent highlighting */
        LuaHost *lua_host = lua_host_create();
        if (lua_host) {
            syntax_ctx.lua_host = lua_host;
            struct loki_lua_opts lua_opts = {
                .bind_editor = 1,
                .load_config = 1,
                .reporter = NULL
            };
            lua_host->L = loki_lua_bootstrap(&syntax_ctx, &lua_opts);
        }

        /* Initialize Link callbacks for REPL notifications */
        shared_repl_link_init_callbacks(ctx.shared);

        repl_loop(&ctx, &syntax_ctx);

        /* Cleanup Link callbacks */
        shared_repl_link_cleanup_callbacks();

        /* Cleanup Lua host */
        if (syntax_ctx.lua_host) {
            lua_host_free(syntax_ctx.lua_host);
            syntax_ctx.lua_host = NULL;
        }
    }

    /* Cleanup */
    alda_async_cleanup();
    alda_tsf_cleanup();
    alda_midi_cleanup(&ctx);
    alda_context_cleanup(&ctx);
    shared_context_cleanup(&shared);

    return result < 0 ? 1 : 0;
}
