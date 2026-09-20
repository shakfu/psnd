/**
 * @file repl.c
 * @brief MHS (Micro Haskell MIDI) REPL and play mode entry points.
 *
 * Provides mhs_repl_main() and mhs_play_main() for psnd CLI dispatch.
 * These wrap the MicroHs main() with appropriate arguments for MIDI support.
 *
 * psnd embeds MHS libraries using VFS (Virtual File System). The VFS
 * intercepts file operations and serves embedded content from memory,
 * making psnd mhs fully self-contained with ~2s startup time.
 *
 * For compilation to executable (-o without .c extension), files are
 * extracted to a temp directory since cc needs real filesystem access.
 *
 * CLI flags for psnd integration:
 *   --virtual NAME   Create virtual MIDI port with given name
 *   -sf PATH         Load soundfont and use built-in synth
 *   -p N             Open MIDI port by index
 *   -l, --list       List available MIDI ports
 *   -v, --verbose    Verbose output
 *
 * Interactive REPL features (via stdin pipe interposition):
 *   - Syntax-highlighted input
 *   - Shared psnd commands (:help, :stop, :panic, :list, etc.)
 *   - Tab completion for Haskell keywords
 *   - History persistence
 *   - Ableton Link callback polling
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#ifndef _WIN32
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <fcntl.h>
#if defined(__APPLE__) || defined(__FreeBSD__)
#include <util.h>
#else
#include <pty.h>
#endif
#endif

#include "vfs.h"
#include "midi_ffi.h"

/* psnd core headers */
#include "psnd.h"       /* from source/core/include */
#include "repl.h"       /* from source/core */
#include "loki/core.h"
#include "loki/internal.h"
#include "loki/syntax.h"
#include "loki/repl_helpers.h"
#include "shared/repl_commands.h"

/* Delay before REPL starts */
#define MHS_REPL_DELAY 30000

/* SharedContext integration for TSF/Csound/Link support */
#ifdef PSND_SHARED_CONTEXT
#include "shared/context.h"
#include "shared/audio/tsf_backend.h"

/* Global SharedContext for REPL mode */
static struct SharedContext *g_mhs_repl_shared = NULL;

/* External setter from midi_ffi.c */
extern void midi_ffi_set_shared(struct SharedContext *ctx);
#endif

/* Forward declaration of MicroHs main */
extern int mhs_main(int argc, char **argv);

/* ============================================================================
 * CLI Argument Parsing
 * ============================================================================ */

typedef struct {
    const char *virtual_name;
    const char *soundfont_path;
    int port_index;
    int list_ports;
    int verbose;
    int show_help;
    /* Remaining args for MHS */
    int mhs_argc;
    char **mhs_argv;
} MhsReplArgs;

/**
 * @brief Parse CLI arguments, separating psnd flags from MHS flags.
 */
static void parse_mhs_args(MhsReplArgs *args, int argc, char **argv) {
    memset(args, 0, sizeof(*args));
    args->port_index = -1;

    /* Allocate space for MHS args (worst case: all args go to MHS) */
    args->mhs_argv = malloc((argc + 1) * sizeof(char *));
    args->mhs_argc = 0;

    for (int i = 0; i < argc; i++) {
        /* psnd-specific flags */
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            args->show_help = 1;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            args->verbose = 1;
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) {
            args->list_ports = 1;
        } else if (strcmp(argv[i], "--virtual") == 0 && i + 1 < argc) {
            args->virtual_name = argv[++i];
        } else if ((strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) &&
                   i + 1 < argc) {
            args->port_index = atoi(argv[++i]);
        } else if ((strcmp(argv[i], "-sf") == 0 || strcmp(argv[i], "--soundfont") == 0) &&
                   i + 1 < argc) {
            args->soundfont_path = argv[++i];
        } else {
            /* Pass through to MHS */
            args->mhs_argv[args->mhs_argc++] = argv[i];
        }
    }
    args->mhs_argv[args->mhs_argc] = NULL;
}

static void free_mhs_args(MhsReplArgs *args) {
    if (args->mhs_argv) {
        free(args->mhs_argv);
        args->mhs_argv = NULL;
    }
}

/* ============================================================================
 * Help and Usage
 * ============================================================================ */

/**
 * @brief Print usage information for psnd mhs.
 */
static void print_mhs_usage(void) {
    printf("psnd mhs - Micro Haskell with MIDI support\n\n");
    printf("Usage:\n");
    printf("  psnd mhs                     Start interactive REPL\n");
    printf("  psnd mhs -r <file.hs>        Run a Haskell file\n");
#ifndef MHS_NO_COMPILATION
    printf("  psnd mhs -o<prog> <file.hs>  Compile to executable\n");
    printf("  psnd mhs -o<file.c> <file.hs> Output C code only\n");
#endif
    printf("  psnd mhs [options]           Pass options to MicroHs\n\n");
    printf("MIDI options:\n");
    printf("  --virtual NAME    Create virtual MIDI port with given name\n");
    printf("  -p N, --port N    Open MIDI port by index\n");
    printf("  -sf PATH          Load soundfont and use built-in synth\n");
    printf("  -l, --list        List available MIDI ports\n");
    printf("  -v, --verbose     Verbose output\n");
    printf("  -h, --help        Show this help\n\n");
    printf("Available MIDI modules: Midi, Music, MusicPerform, MidiPerform, Async\n\n");
    printf("Examples:\n");
    printf("  psnd mhs                     Start REPL with default virtual port\n");
    printf("  psnd mhs --virtual MyPort    Start REPL with named virtual port\n");
    printf("  psnd mhs -sf gm.sf2          Start REPL with built-in synth\n");
    printf("  psnd mhs -p 0                Start REPL using MIDI port 0\n");
    printf("  psnd mhs -r MyFile.hs        Run a Haskell file\n");
#ifndef MHS_NO_COMPILATION
    printf("  psnd mhs -oMyProg MyFile.hs  Compile to executable\n\n");
#endif
    printf("MicroHs options: -q (quiet), -C (cache), -i<path> (include)\n");
}

/**
 * @brief Print MHS-specific REPL help.
 */
static void print_mhs_repl_help(void) {
    shared_print_command_help();

    printf("MHS-specific:\n");
    printf("  Lines are passed to MicroHs REPL for evaluation\n");
    printf("  MicroHs commands: :quit, :type, :kind, :browse, etc.\n");
    printf("\n");
    printf("Haskell Quick Reference:\n");
    printf("  import Midi           Import MIDI module\n");
    printf("  midiInit              Initialize MIDI (auto-called)\n");
    printf("  midiNoteOn c p v      Note on (channel, pitch, velocity)\n");
    printf("  midiNoteOff c p       Note off (channel, pitch)\n");
    printf("  midiCC c cc v         Control change\n");
    printf("  midiSleep ms          Sleep for milliseconds\n");
    printf("  midiPanic             All notes off\n");
    printf("\n");
}

/**
 * @brief List available MIDI ports.
 */
static void list_midi_ports(void) {
    if (mhs_midi_init() != 0) {
        fprintf(stderr, "Error: Failed to initialize MIDI\n");
        return;
    }

    int count = midi_list_ports();
    printf("MIDI outputs:\n");
    if (count == 0) {
        printf("  (none)\n");
    } else {
        for (int i = 0; i < count; i++) {
            printf("  %d: %s\n", i, midi_port_name(i));
        }
    }

    mhs_midi_cleanup();
}

/* ============================================================================
 * MIDI/Audio Setup
 * ============================================================================ */

#ifdef PSND_SHARED_CONTEXT
/**
 * @brief Initialize SharedContext and MIDI for REPL mode.
 */
static int setup_midi_for_repl(MhsReplArgs *args) {
    /* Allocate SharedContext */
    g_mhs_repl_shared = (struct SharedContext *)calloc(1, sizeof(struct SharedContext));
    if (!g_mhs_repl_shared) {
        fprintf(stderr, "Error: Failed to allocate shared context\n");
        return -1;
    }

    if (shared_context_init(g_mhs_repl_shared) != 0) {
        fprintf(stderr, "Error: Failed to initialize shared context\n");
        free(g_mhs_repl_shared);
        g_mhs_repl_shared = NULL;
        return -1;
    }

    /* Route midi_ffi through SharedContext for TSF/Csound/Link support */
    midi_ffi_set_shared(g_mhs_repl_shared);

    /* Initialize MIDI subsystem */
    if (mhs_midi_init() != 0) {
        fprintf(stderr, "Error: Failed to initialize MIDI\n");
        shared_context_cleanup(g_mhs_repl_shared);
        free(g_mhs_repl_shared);
        g_mhs_repl_shared = NULL;
        return -1;
    }

    /* Setup audio/MIDI output based on args */
    if (args->soundfont_path) {
        /* Use built-in synth */
        if (shared_audio_tsf_load(args->soundfont_path) != 0) {
            fprintf(stderr, "Error: Failed to load soundfont: %s\n", args->soundfont_path);
            mhs_midi_cleanup();
            shared_context_cleanup(g_mhs_repl_shared);
            free(g_mhs_repl_shared);
            g_mhs_repl_shared = NULL;
            return -1;
        }
        if (shared_audio_tsf_enable(g_mhs_repl_shared) != 0) {
            fprintf(stderr, "Error: Failed to enable built-in synth\n");
            mhs_midi_cleanup();
            shared_context_cleanup(g_mhs_repl_shared);
            free(g_mhs_repl_shared);
            g_mhs_repl_shared = NULL;
            return -1;
        }
        if (args->verbose) {
            printf("Using built-in synth: %s\n", args->soundfont_path);
        }
    } else {
        /* Setup MIDI output */
        int midi_opened = 0;

        if (args->virtual_name) {
            if (midi_open_virtual(args->virtual_name) == 0) {
                midi_opened = 1;
                if (args->verbose) {
                    printf("Created virtual MIDI output: %s\n", args->virtual_name);
                }
            }
        } else if (args->port_index >= 0) {
            if (midi_open(args->port_index) == 0) {
                midi_opened = 1;
                if (args->verbose) {
                    printf("Opened MIDI port %d\n", args->port_index);
                }
            }
        } else {
            /* Default: try to open virtual port with psnd name */
            if (midi_open_virtual(PSND_MIDI_PORT_NAME) == 0) {
                midi_opened = 1;
                if (args->verbose) {
                    printf("Created virtual MIDI output: " PSND_MIDI_PORT_NAME "\n");
                }
            }
        }

        if (!midi_opened) {
            fprintf(stderr, "Warning: No MIDI output available\n");
            fprintf(stderr, "Hint: Use -sf <soundfont.sf2> for built-in synth\n");
        }
    }

    return 0;
}

/**
 * @brief Cleanup SharedContext and MIDI for REPL mode.
 */
static void cleanup_midi_for_repl(void) {
    /* Send panic to stop all notes */
    midi_panic();

    /* Clear SharedContext routing */
    midi_ffi_set_shared(NULL);

    /* Cleanup MIDI */
    mhs_midi_cleanup();

    /* Free SharedContext */
    if (g_mhs_repl_shared) {
        shared_context_cleanup(g_mhs_repl_shared);
        free(g_mhs_repl_shared);
        g_mhs_repl_shared = NULL;
    }
}
#else
/* Stub implementations when SharedContext is not available */
static int setup_midi_for_repl(MhsReplArgs *args) {
    if (mhs_midi_init() != 0) {
        fprintf(stderr, "Error: Failed to initialize MIDI\n");
        return -1;
    }

    int midi_opened = 0;
    if (args->virtual_name) {
        midi_opened = (midi_open_virtual(args->virtual_name) == 0);
    } else if (args->port_index >= 0) {
        midi_opened = (midi_open(args->port_index) == 0);
    } else {
        midi_opened = (midi_open_virtual(PSND_MIDI_PORT_NAME) == 0);
    }

    if (!midi_opened && args->verbose) {
        fprintf(stderr, "Warning: No MIDI output available\n");
    }

    return 0;
}

static void cleanup_midi_for_repl(void) {
    midi_panic();
    mhs_midi_cleanup();
}
#endif

/* Cross-platform setenv */
static int set_env(const char *name, const char *value) {
#ifdef _WIN32
    return _putenv_s(name, value);
#else
    return setenv(name, value, 1);
#endif
}

/* ============================================================================
 * Interactive REPL with Stdin Pipe
 * ============================================================================ */

/* Haskell keywords for tab completion (without type suffix markers) */
static const char *mhs_completion_words[] = {
    /* Reserved words */
    "module", "import", "qualified", "as", "hiding",
    "where", "let", "in", "case", "of", "if", "then", "else",
    "do", "return", "class", "instance", "data", "type", "newtype",
    "deriving", "default", "infix", "infixl", "infixr",
    /* Common functions */
    "main", "print", "putStrLn", "putStr", "show", "read",
    "map", "filter", "foldr", "foldl", "zip", "unzip",
    "head", "tail", "init", "last", "length", "null", "reverse",
    "take", "drop", "concat", "sum", "product", "maximum", "minimum",
    "and", "or", "not", "otherwise",
    /* Monadic */
    "pure", "fmap", "bind", "sequence", "mapM", "forM",
    /* MHS MIDI primitives */
    "midiInit", "midiCleanup", "midiListPorts", "midiPortCount",
    "midiOpenPort", "midiOpenVirtual", "midiClose",
    "midiNoteOn", "midiNoteOff", "midiCC", "midiProgramChange",
    "midiPitchBend", "midiPanic", "midiSleep",
    /* Music module */
    "note", "rest", "chord", "line", "times", "tempo",
    "instrument", "dynamic", "phrase", "cut", "remove",
    /* Types */
    "Int", "Integer", "Float", "Double", "Bool", "Char", "String",
    "Maybe", "Either", "IO", "List", "Monad", "Functor", "Applicative",
    "True", "False", "Just", "Nothing", "Left", "Right",
    NULL
};

/* PTY master file descriptor for communicating with MHS */
static int g_mhs_pty_master = -1;

/* Child process PID for MHS */
static pid_t g_mhs_child_pid = -1;

/* Original terminal settings (to restore on exit) */
static struct termios g_orig_termios;
static int g_termios_saved = 0;

/**
 * @brief Stop callback for MHS REPL - sends panic.
 */
static void mhs_stop_playback(void) {
    midi_panic();
}

/**
 * @brief Process MHS REPL command.
 *
 * @return 0=continue (command handled), 1=quit, 2=pass to MicroHs
 */
static int mhs_process_command(const char *input) {
    const char *cmd = input;
    if (cmd[0] == ':') cmd++;

#ifdef PSND_SHARED_CONTEXT
    int result = shared_process_command(g_mhs_repl_shared, input, mhs_stop_playback);
#else
    int result = REPL_CMD_NOT_CMD;
    /* Basic command handling without SharedContext */
    if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "q") == 0 ||
        strcmp(cmd, "exit") == 0) {
        return 1; /* quit */
    }
    if (strcmp(cmd, "panic") == 0 || strcmp(cmd, "p") == 0) {
        midi_panic();
        return 0;
    }
#endif

    if (result == REPL_CMD_QUIT) {
        return 1; /* quit */
    }
    if (result == REPL_CMD_HANDLED) {
        return 0; /* command handled */
    }

    /* Handle MHS-specific help command */
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0 || strcmp(cmd, "?") == 0) {
        print_mhs_repl_help();
        return 0;
    }

    return 2; /* pass to MicroHs */
}

/**
 * @brief Tab completion callback for MHS REPL.
 */
static char **mhs_completion_callback(const char *prefix, int *count, void *user_data) {
    (void)user_data;

    if (!prefix || !count) return NULL;

    size_t prefix_len = strlen(prefix);
    if (prefix_len == 0) {
        *count = 0;
        return NULL;
    }

    /* Count matches */
    int matches = 0;
    for (int i = 0; mhs_completion_words[i]; i++) {
        if (strncmp(mhs_completion_words[i], prefix, prefix_len) == 0) {
            matches++;
        }
    }

    if (matches == 0) {
        *count = 0;
        return NULL;
    }

    /* Allocate result array */
    char **result = malloc((matches + 1) * sizeof(char *));
    if (!result) {
        *count = 0;
        return NULL;
    }

    /* Fill matches */
    int idx = 0;
    for (int i = 0; mhs_completion_words[i] && idx < matches; i++) {
        if (strncmp(mhs_completion_words[i], prefix, prefix_len) == 0) {
            result[idx++] = strdup(mhs_completion_words[i]);
        }
    }
    result[idx] = NULL;

    *count = matches;
    return result;
}

/**
 * @brief Check if MHS child process is still running.
 */
static int mhs_child_running(void) {
    if (g_mhs_child_pid <= 0) return 0;
    int status;
    pid_t result = waitpid(g_mhs_child_pid, &status, WNOHANG);
    if (result == 0) return 1;  /* Still running */
    if (result == g_mhs_child_pid) return 0;  /* Exited */
    return 0;  /* Error or not our child */
}

/**
 * @brief Read available output from MHS PTY (non-blocking).
 *
 * Reads and prints any output from MicroHs to stdout.
 */
static void mhs_read_output(void) {
    if (g_mhs_pty_master < 0) return;

    char buf[1024];
    ssize_t n;

    /* Set non-blocking temporarily */
    int flags = fcntl(g_mhs_pty_master, F_GETFL, 0);
    fcntl(g_mhs_pty_master, F_SETFL, flags | O_NONBLOCK);

    while ((n = read(g_mhs_pty_master, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        /* Filter out MicroHs prompt since we show our own */
        char *p = buf;
        while (*p) {
            /* Skip "> " prompt from MicroHs */
            if (p[0] == '>' && p[1] == ' ') {
                p += 2;
                continue;
            }
            putchar(*p++);
        }
        fflush(stdout);
    }

    /* Restore blocking mode */
    fcntl(g_mhs_pty_master, F_SETFL, flags);
}

/**
 * @brief Run the interactive MHS REPL with PTY interposition.
 *
 * This function:
 * 1. Creates a pseudo-terminal (PTY) for MicroHs
 * 2. Forks a child process to run MicroHs with PTY as its terminal
 * 3. Child initializes MIDI (must happen after fork for libremidi to work)
 * 4. Parent runs psnd's REPL loop with syntax highlighting and completion
 * 5. Processes psnd commands locally, forwards others to MicroHs via PTY
 *
 * Using a PTY instead of a pipe allows MicroHs's SimpleReadline to work
 * because it sees a real terminal (tcgetattr succeeds).
 */
static int run_mhs_interactive_repl(int mhs_argc, char **mhs_argv, MhsReplArgs *args) {
    int result = 0;
    struct termios slave_termios;
    struct winsize ws;

    /* Get current terminal settings to copy to PTY */
    if (tcgetattr(STDIN_FILENO, &g_orig_termios) == 0) {
        g_termios_saved = 1;
        slave_termios = g_orig_termios;
    } else {
        /* Fallback: use sensible defaults */
        memset(&slave_termios, 0, sizeof(slave_termios));
        slave_termios.c_iflag = ICRNL | IXON;
        slave_termios.c_oflag = OPOST | ONLCR;
        slave_termios.c_cflag = CS8 | CREAD | CLOCAL;
        slave_termios.c_lflag = ISIG | ICANON | ECHO | ECHOE | ECHOK;
        cfsetispeed(&slave_termios, B9600);
        cfsetospeed(&slave_termios, B9600);
    }

    /* Get window size */
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) != 0) {
        ws.ws_row = 24;
        ws.ws_col = 80;
        ws.ws_xpixel = 0;
        ws.ws_ypixel = 0;
    }

    /* Fork with PTY - child gets slave side as stdin/stdout/stderr */
    g_mhs_child_pid = forkpty(&g_mhs_pty_master, NULL, &slave_termios, &ws);

    if (g_mhs_child_pid == -1) {
        fprintf(stderr, "Error: Failed to forkpty\n");
        return 1;
    }

    if (g_mhs_child_pid == 0) {
        /* Child process: run MicroHs with PTY as terminal */
        /* forkpty already set up stdin/stdout/stderr to the slave PTY */

        /* Initialize MIDI in child process (must happen after fork) */
        if (setup_midi_for_repl(args) != 0) {
            fprintf(stderr, "Warning: MIDI initialization failed in child\n");
        }

        /* Run MicroHs */
        int mhs_result = mhs_main(mhs_argc, mhs_argv);

        /* Cleanup MIDI before exit */
        cleanup_midi_for_repl();
        _exit(mhs_result);
    }

    /* Parent process: run psnd REPL, communicate via PTY master */

    /* Initialize Link callbacks */
#ifdef PSND_SHARED_CONTEXT
    shared_repl_link_init_callbacks(g_mhs_repl_shared);
#endif

    /* Initialize REPL editor */
    ReplLineEditor ed;
#ifdef LOKI_USE_LINENOISE
    repl_editor_init_with_language(&ed, REPL_LANG_HASKELL);
#else
    repl_editor_init(&ed);
#endif

    /* Set up syntax highlighting context */
    editor_ctx_t syntax_ctx;
    editor_ctx_init(&syntax_ctx);
    syntax_init_default_colors(&syntax_ctx);
    /* Select Haskell syntax highlighting using dummy filename */
    syntax_select_for_filename(&syntax_ctx, "input.hs");

    /* Set up tab completion */
    repl_set_completion(&ed, mhs_completion_callback, NULL);

    /* Load history */
    char history_path[512] = {0};
    if (repl_get_history_path("mhs", history_path, sizeof(history_path))) {
        repl_history_load(&ed, history_path);
    }

    if (args->verbose) {
        printf("MHS REPL %s (type :help for psnd commands)\n", PSND_VERSION);
    }

    /* Wait for MicroHs to start and print welcome message.
     * MicroHs needs time to load cache (~2s) before printing welcome.
     * Poll the PTY until we get output, with a reasonable timeout. */
    {
        int got_output = 0;
        for (int i = 0; i < 100; i++) {  /* Up to 5 seconds */
            usleep(MHS_REPL_DELAY); /* 50ms */

            mhs_read_output();

            /* After first output, wait a bit more for the rest */
            if (!got_output) {
                /* Use select to check for data */
                fd_set fds;
                struct timeval tv = {0, 0};
                FD_ZERO(&fds);
                FD_SET(g_mhs_pty_master, &fds);
                if (select(g_mhs_pty_master + 1, &fds, NULL, NULL, &tv) > 0) {
                    got_output = 1;
                }
            } else if (i > 10) {
                /* Got output, waited 500ms more, proceed */
                break;
            }
        }
    }

    /* Enable raw mode for syntax-highlighted input */
    repl_enable_raw_mode();

    /* Main REPL loop */
    while (mhs_child_running()) {
        char *input = repl_readline(&syntax_ctx, &ed, "mhs> ");

        if (input == NULL) {
            /* EOF - signal MHS to quit */
            break;
        }

        if (input[0] == '\0') {
            /* Empty line - still send newline to MHS */
            if (g_mhs_pty_master >= 0) {
                write(g_mhs_pty_master, "\n", 1);
            }
            usleep(50000);
            mhs_read_output();
            continue;
        }

        repl_add_history(&ed, input);

        /* Process command */
        int cmd_result = mhs_process_command(input);

        if (cmd_result == 1) {
            /* Quit requested */
            break;
        }

        if (cmd_result == 0) {
            /* Command handled by psnd */
#ifdef PSND_SHARED_CONTEXT
            shared_repl_link_check();
#endif
            continue;
        }

        /* Pass input to MicroHs via PTY */
        if (g_mhs_pty_master >= 0) {
            write(g_mhs_pty_master, input, strlen(input));
            write(g_mhs_pty_master, "\n", 1);
        }

        /* Give MicroHs time to process and output */
        usleep(100000); /* 100ms for output to appear */
        mhs_read_output();

        /* Poll Link callbacks */
#ifdef PSND_SHARED_CONTEXT
        shared_repl_link_check();
#endif
    }

    /* Disable raw mode */
    repl_disable_raw_mode();

    /* Save history */
    if (history_path[0]) {
        repl_history_save(&ed, history_path);
    }

    /* Cleanup editor */
    repl_editor_cleanup(&ed);
    /* Note: syntax_ctx doesn't have Lua host, so minimal cleanup needed */

    /* Signal MHS to exit by sending :quit and closing PTY */
    if (g_mhs_pty_master >= 0) {
        const char *quit_cmd = ":quit\n";
        write(g_mhs_pty_master, quit_cmd, strlen(quit_cmd));
        close(g_mhs_pty_master);
        g_mhs_pty_master = -1;
    }

    /* Wait for MHS child to finish */
    if (g_mhs_child_pid > 0) {
        int status;
        waitpid(g_mhs_child_pid, &status, 0);
        if (WIFEXITED(status)) {
            result = WEXITSTATUS(status);
        }
        g_mhs_child_pid = -1;
    }

    /* Cleanup Link callbacks */
#ifdef PSND_SHARED_CONTEXT
    shared_repl_link_cleanup_callbacks();
#endif

    return result;
}

/* ============================================================================
 * Build MHS argv with VFS paths
 * ============================================================================ */

#ifndef MHS_NO_COMPILATION
/**
 * @brief Check if we need to extract files for compilation.
 *
 * When compiling to an executable (not .c output), cc needs real files.
 *
 * @return 1 if extraction needed (executable output), 0 otherwise
 */
static int needs_extraction(int argc, char **argv) {
    for (int i = 0; i < argc; i++) {
        /* Check for -oFILE or -o FILE */
        if (strncmp(argv[i], "-o", 2) == 0) {
            const char *output = NULL;
            if (argv[i][2] != '\0') {
                /* -oFILE form */
                output = argv[i] + 2;
            } else if (i + 1 < argc) {
                /* -o FILE form */
                output = argv[i + 1];
            }
            if (output) {
                size_t len = strlen(output);
                /* Check if output ends with .c */
                if (len >= 2 && strcmp(output + len - 2, ".c") == 0) {
                    return 0;  /* C output, VFS works fine */
                }
                return 1;  /* Executable output, needs extraction */
            }
        }
    }
    return 0;  /* No -o flag, VFS works fine */
}
#endif /* MHS_NO_COMPILATION */

#ifdef MHS_USE_PKG
#define MHS_CACHE_FILE ".mhscache"

/* Path of the running psnd binary, which is where the packages are embedded. */
static int mhs_exe_path(char *buf, size_t size) {
#if defined(__APPLE__)
    uint32_t n = (uint32_t)size;
    return _NSGetExecutablePath(buf, &n) == 0 ? 0 : -1;
#elif defined(__linux__)
    ssize_t n = readlink("/proc/self/exe", buf, size - 1);
    if (n <= 0) return -1;
    buf[n] = '\0';
    return 0;
#else
    (void)buf;
    (void)size;
    return -1;
#endif
}

/* MicroHs appends the -p packages to .mhscache on every run without checking
   whether the cache it just read already holds them, so preloading against a
   warm cache costs ~2MB and a second of load time per run, without bound. The
   packages survive in the cache, so only preload when there is nothing usable
   to reuse. */
static int mhs_need_preload(void) {
    struct stat cache_st;
    if (stat(MHS_CACHE_FILE, &cache_st) != 0) return 1;

    /* A run killed mid-save leaves a zero-length cache. mhs then aborts on it
       and never rewrites it, so drop it here. */
    if (cache_st.st_size == 0) {
        remove(MHS_CACHE_FILE);
        return 1;
    }

    char exe[1024];
    struct stat exe_st;
    if (mhs_exe_path(exe, sizeof(exe)) != 0 || stat(exe, &exe_st) != 0) return 1;

    /* A newer binary can carry different packages, and the cached copies are
       never validated against it. Start over rather than mix the two. */
    if (exe_st.st_mtime >= cache_st.st_mtime) {
        remove(MHS_CACHE_FILE);
        return 1;
    }
    return 0;
}
#endif /* MHS_USE_PKG */

static char **build_mhs_argv(MhsReplArgs *args, int *out_argc, char *path_buf1,
                             char *path_buf2, size_t buf_size) {
    /* Calculate total args needed */
#ifdef MHS_USE_PKG
    int extra_args = 4;  /* -C, -a<path>, and up to two -p flags */
#else
    int extra_args = 3;  /* -C, -i<path>, -i<path>/lib */
#endif

    int new_argc = args->mhs_argc + extra_args;
    char **new_argv = malloc((new_argc + 1) * sizeof(char *));
    if (!new_argv) return NULL;

    int j = 0;
    new_argv[j++] = "mhs";
    new_argv[j++] = "-C";  /* Enable caching */

#ifdef MHS_USE_PKG
    snprintf(path_buf1, buf_size, "-a%s", VFS_VIRTUAL_ROOT);
    new_argv[j++] = path_buf1;
    if (mhs_need_preload()) {
        new_argv[j++] = "-pbase";
        new_argv[j++] = "-pmusic";
    }
#else
    snprintf(path_buf1, buf_size, "-i%s", VFS_VIRTUAL_ROOT);
    snprintf(path_buf2, buf_size, "-i%s/lib", VFS_VIRTUAL_ROOT);
    new_argv[j++] = path_buf1;
    new_argv[j++] = path_buf2;
#endif

    /* Copy user's MHS arguments (skip argv[0] which is already "mhs") */
    for (int i = 1; i < args->mhs_argc; i++) {
        new_argv[j++] = args->mhs_argv[i];
    }
    new_argv[j] = NULL;

    *out_argc = j;
    return new_argv;
}

/**
 * @brief Check if MHS will run in REPL mode (no -r or file argument).
 */
static int is_repl_mode(MhsReplArgs *args) {
    for (int i = 1; i < args->mhs_argc; i++) {
        /* Check for -r (run) flag */
        if (strcmp(args->mhs_argv[i], "-r") == 0) {
            return 0;
        }
        /* Check for -o (compile) flag */
        if (strncmp(args->mhs_argv[i], "-o", 2) == 0) {
            return 0;
        }
        /* Check for .hs file argument (not a flag) */
        if (args->mhs_argv[i][0] != '-') {
            const char *ext = strrchr(args->mhs_argv[i], '.');
            if (ext && (strcmp(ext, ".hs") == 0 || strcmp(ext, ".mhs") == 0)) {
                return 0;
            }
        }
    }
    return 1; /* No file or -r flag, so REPL mode */
}

/* ============================================================================
 * MHS REPL Main Entry Point
 * ============================================================================ */

/**
 * @brief MHS REPL entry point.
 *
 * Called when user runs: psnd mhs
 * Starts an interactive MicroHs REPL with MIDI library support.
 * Uses embedded VFS for fast startup (~2s vs ~17s from source).
 *
 * For interactive mode on a TTY, uses stdin pipe interposition to provide:
 * - Syntax-highlighted input
 * - Shared psnd commands
 * - Tab completion
 * - History persistence
 */
int mhs_repl_main(int argc, char **argv) {
    /* Parse arguments */
    MhsReplArgs args;
    parse_mhs_args(&args, argc, argv);

    /* Handle --help */
    if (args.show_help) {
        print_mhs_usage();
        free_mhs_args(&args);
        return 0;
    }

    /* Handle --list */
    if (args.list_ports) {
        list_midi_ports();
        free_mhs_args(&args);
        return 0;
    }

    /* Initialize VFS with embedded libraries */
    if (vfs_init() != 0) {
        fprintf(stderr, "Error: Failed to initialize MHS Virtual File System\n");
        free_mhs_args(&args);
        return 1;
    }

    /* Check if running in interactive REPL mode on a TTY */
    int interactive_repl = is_repl_mode(&args) && isatty(STDIN_FILENO);

    /* Setup MIDI/audio - but NOT for interactive mode (child will do it after fork) */
    if (!interactive_repl) {
        if (setup_midi_for_repl(&args) != 0) {
            free_mhs_args(&args);
            return 1;
        }
    }

#ifdef MHS_NO_COMPILATION
    /*
     * Compilation disabled - simple VFS-only path
     */

    /* Check if user is trying to compile to executable */
    for (int i = 0; i < args.mhs_argc; i++) {
        if (strncmp(args.mhs_argv[i], "-o", 2) == 0) {
            const char *output = NULL;
            if (args.mhs_argv[i][2] != '\0') {
                output = args.mhs_argv[i] + 2;
            } else if (i + 1 < args.mhs_argc) {
                output = args.mhs_argv[i + 1];
            }
            if (output) {
                size_t len = strlen(output);
                if (len < 2 || strcmp(output + len - 2, ".c") != 0) {
                    fprintf(stderr, "Error: Compilation to executable is disabled in this build.\n");
                    fprintf(stderr, "This psnd was built with MHS_ENABLE_COMPILATION=OFF.\n");
                    fprintf(stderr, "\n");
                    fprintf(stderr, "Available options:\n");
                    fprintf(stderr, "  psnd mhs -o%s.c %s   Output C code only\n",
                            output, args.mhs_argc > i + 2 ? args.mhs_argv[i + 2] : "file.hs");
                    fprintf(stderr, "  psnd mhs -r file.hs       Run without compiling\n");
                    fprintf(stderr, "\n");
                    fprintf(stderr, "To enable compilation, rebuild psnd with:\n");
                    fprintf(stderr, "  cmake -DMHS_ENABLE_COMPILATION=ON ..\n");
                    cleanup_midi_for_repl();
                    free_mhs_args(&args);
                    return 1;
                }
            }
        }
    }

    set_env("MHSDIR", VFS_VIRTUAL_ROOT);

    char path_buf1[512], path_buf2[512];
    int new_argc;
    char **new_argv = build_mhs_argv(&args, &new_argc, path_buf1, path_buf2, sizeof(path_buf1));
    if (!new_argv) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        cleanup_midi_for_repl();
        free_mhs_args(&args);
        return 1;
    }

    int result;
    if (interactive_repl) {
        /* Use enhanced REPL with PTY - MIDI init happens in child after fork */
        result = run_mhs_interactive_repl(new_argc, new_argv, &args);
    } else {
        /* Non-interactive: run MHS directly */
        result = mhs_main(new_argc, new_argv);
    }

    free(new_argv);
    if (!interactive_repl) {
        cleanup_midi_for_repl();
    }
    free_mhs_args(&args);
    return result;

#else /* MHS_NO_COMPILATION not defined - full compilation support */

    char *temp_dir = NULL;
    int linking_midi = 0;

    /* Check if we're compiling to an executable (cc needs real files) */
    if (needs_extraction(args.mhs_argc, args.mhs_argv)) {
        temp_dir = vfs_extract_to_temp();
        if (!temp_dir) {
            fprintf(stderr, "Error: Failed to extract embedded files for compilation\n");
            cleanup_midi_for_repl();
            free_mhs_args(&args);
            return 1;
        }
        /* Set MHSDIR to temp directory for cc to find runtime files */
        set_env("MHSDIR", temp_dir);
        linking_midi = 1;
        interactive_repl = 0; /* Compilation mode, not REPL */
    } else {
        /* Use VFS - set MHSDIR to virtual root */
        set_env("MHSDIR", VFS_VIRTUAL_ROOT);
    }

    /* Build argv for MHS */
#ifdef __APPLE__
    #define LINK_EXTRA_ARGS 14
#else
    #define LINK_EXTRA_ARGS 14
#endif

#ifdef MHS_USE_PKG
    int extra_args = 4;
#else
    int extra_args = 3;
#endif
    if (linking_midi) {
        extra_args += LINK_EXTRA_ARGS;
    }

    int new_argc = args.mhs_argc + extra_args;
    char **new_argv = malloc((new_argc + 1) * sizeof(char *));

    char path_arg1[512];
    char path_arg2[512];
    char lib_libremidi[512];
    char lib_midi_ffi[512];
    char lib_music_theory[512];

    if (!new_argv) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        if (temp_dir) vfs_cleanup_temp(temp_dir);
        cleanup_midi_for_repl();
        free_mhs_args(&args);
        return 1;
    }

    int j = 0;
    new_argv[j++] = "mhs";
    new_argv[j++] = "-C";

#ifdef MHS_USE_PKG
    if (temp_dir) {
        snprintf(path_arg1, sizeof(path_arg1), "-a%s", temp_dir);
    } else {
        snprintf(path_arg1, sizeof(path_arg1), "-a%s", VFS_VIRTUAL_ROOT);
    }
    new_argv[j++] = path_arg1;
    if (mhs_need_preload()) {
        new_argv[j++] = "-pbase";
        new_argv[j++] = "-pmusic";
    }
#else
    if (temp_dir) {
        snprintf(path_arg1, sizeof(path_arg1), "-i%s", temp_dir);
        snprintf(path_arg2, sizeof(path_arg2), "-i%s/lib", temp_dir);
    } else {
        snprintf(path_arg1, sizeof(path_arg1), "-i%s", VFS_VIRTUAL_ROOT);
        snprintf(path_arg2, sizeof(path_arg2), "-i%s/lib", VFS_VIRTUAL_ROOT);
    }
    new_argv[j++] = path_arg1;
    new_argv[j++] = path_arg2;
#endif

    if (linking_midi) {
        snprintf(lib_midi_ffi, sizeof(lib_midi_ffi), "%s/lib/libmidi_ffi.a", temp_dir);
        snprintf(lib_music_theory, sizeof(lib_music_theory), "%s/lib/libmusic_theory.a", temp_dir);
        snprintf(lib_libremidi, sizeof(lib_libremidi), "%s/lib/liblibremidi.a", temp_dir);

        new_argv[j++] = "-optl";
        new_argv[j++] = lib_midi_ffi;
        new_argv[j++] = "-optl";
        new_argv[j++] = lib_music_theory;
        new_argv[j++] = "-optl";
        new_argv[j++] = lib_libremidi;

#ifdef __APPLE__
        new_argv[j++] = "-optl";
        new_argv[j++] = "-framework";
        new_argv[j++] = "-optl";
        new_argv[j++] = "CoreMIDI";
        new_argv[j++] = "-optl";
        new_argv[j++] = "-framework";
        new_argv[j++] = "-optl";
        new_argv[j++] = "CoreFoundation";
        new_argv[j++] = "-optl";
        new_argv[j++] = "-framework";
        new_argv[j++] = "-optl";
        new_argv[j++] = "CoreAudio";
        new_argv[j++] = "-optl";
        new_argv[j++] = "-lc++";
#else
        new_argv[j++] = "-optl";
        new_argv[j++] = "-Wl,--no-as-needed";
        new_argv[j++] = "-optl";
        new_argv[j++] = "-lasound";
        new_argv[j++] = "-optl";
        new_argv[j++] = "-lstdc++";
        new_argv[j++] = "-optl";
        new_argv[j++] = "-lm";
#endif
    }

    for (int i = 1; i < args.mhs_argc; i++) {
        new_argv[j++] = args.mhs_argv[i];
    }
    new_argv[j] = NULL;
    new_argc = j;

    int result;
    if (interactive_repl) {
        /* Use enhanced REPL with PTY - MIDI init happens in child after fork */
        result = run_mhs_interactive_repl(new_argc, new_argv, &args);
    } else {
        /* Non-interactive: run MHS directly */
        result = mhs_main(new_argc, new_argv);
    }

    free(new_argv);

    if (temp_dir) {
        vfs_cleanup_temp(temp_dir);
    }

    if (!interactive_repl) {
        cleanup_midi_for_repl();
    }
    free_mhs_args(&args);

    return result;
#endif /* MHS_NO_COMPILATION */
}

/* ============================================================================
 * MHS Play Main Entry Point
 * ============================================================================ */

/**
 * @brief MHS play entry point.
 *
 * Called when user runs: psnd play file.hs
 * Runs the specified Haskell file.
 * Uses embedded VFS for fast startup.
 */
int mhs_play_main(int argc, char **argv) {
    if (argc < 1) {
        fprintf(stderr, "Usage: psnd play [-v] [-sf soundfont.sf2] <file.hs>\n");
        return 1;
    }

    /* Parse arguments for play mode */
    MhsReplArgs args;
    memset(&args, 0, sizeof(args));
    args.port_index = -1;

    const char *input_file = NULL;

    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            args.verbose = 1;
        } else if ((strcmp(argv[i], "-sf") == 0 || strcmp(argv[i], "--soundfont") == 0) &&
                   i + 1 < argc) {
            args.soundfont_path = argv[++i];
        } else if (strcmp(argv[i], "--virtual") == 0 && i + 1 < argc) {
            args.virtual_name = argv[++i];
        } else if (argv[i][0] != '-' && input_file == NULL) {
            input_file = argv[i];
        }
    }

    if (!input_file) {
        fprintf(stderr, "Usage: psnd play [-v] [-sf soundfont.sf2] <file.hs>\n");
        return 1;
    }

    /* Initialize VFS */
    if (vfs_init() != 0) {
        fprintf(stderr, "Error: Failed to initialize MHS Virtual File System\n");
        return 1;
    }

    /* Setup MIDI/audio */
    if (setup_midi_for_repl(&args) != 0) {
        return 1;
    }

    /* Set MHSDIR to VFS virtual root */
    set_env("MHSDIR", VFS_VIRTUAL_ROOT);

    /* Build argv for MHS run */
#ifdef MHS_USE_PKG
    int extra_args = 5;
#else
    int extra_args = 4;
#endif
    int new_argc = 1 + extra_args + 1;
    char **new_argv = malloc((new_argc + 1) * sizeof(char *));
    char path_arg1[512];
    char path_arg2[512];

    if (!new_argv) {
        fprintf(stderr, "Error: Memory allocation failed\n");
        cleanup_midi_for_repl();
        return 1;
    }

    int j = 0;
    new_argv[j++] = "mhs";
    new_argv[j++] = "-C";

#ifdef MHS_USE_PKG
    snprintf(path_arg1, sizeof(path_arg1), "-a%s", VFS_VIRTUAL_ROOT);
    new_argv[j++] = path_arg1;
    if (mhs_need_preload()) {
        new_argv[j++] = "-pbase";
        new_argv[j++] = "-pmusic";
    }
#else
    snprintf(path_arg1, sizeof(path_arg1), "-i%s", VFS_VIRTUAL_ROOT);
    snprintf(path_arg2, sizeof(path_arg2), "-i%s/lib", VFS_VIRTUAL_ROOT);
    new_argv[j++] = path_arg1;
    new_argv[j++] = path_arg2;
#endif
    new_argv[j++] = "-r";
    new_argv[j++] = (char *)input_file;
    new_argv[j] = NULL;

    if (args.verbose) {
        printf("Running: %s\n", input_file);
    }

    int result = mhs_main(j, new_argv);

    free(new_argv);
    cleanup_midi_for_repl();

    return result;
}
