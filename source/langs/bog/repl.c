/**
 * @file repl.c
 * @brief Bog REPL - Interactive Prolog-based music live coding environment.
 *
 * Provides a standalone REPL for the Bog language with:
 * - Live code evaluation with quantized transitions
 * - MIDI output via SharedContext
 * - Command-line interface similar to other psnd languages
 */

#include "bog_repl.h"
#include "bog_async.h"
#include "repl.h"
#include "psnd.h"
#include "loki/core.h"
#include "loki/internal.h"
#include "loki/syntax.h"
#include "loki/soundfont.h"
#include "loki/lua.h"
#include "loki/repl_launcher.h"
#include "loki/repl_helpers.h"
#include "shared/repl_commands.h"
#include "shared/context.h"
#include "shared/midi/midi.h"
#include "shared/audio/audio.h"

#include <lua.h>

/* Bog library headers */
#include "bog.h"
#include "scheduler.h"
#include "livecoding.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define usleep(us) Sleep((us) / 1000)
#define isatty _isatty
#define STDIN_FILENO 0
#else
#include <unistd.h>
#include <signal.h>
#endif

/* ============================================================================
 * Bog Usage and Help
 * ============================================================================ */

static void print_bog_repl_usage(const char *prog) {
    printf("Usage: %s bog [options] [file.bog]\n", prog);
    printf("\n");
    printf("Bog - Prolog-based music live coding language.\n");
    printf("If no file is provided, starts an interactive REPL.\n");
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help        Show this help message\n");
    printf("  -v, --verbose     Enable verbose output\n");
    printf("  -l, --list        List available MIDI ports\n");
    printf("  -p, --port N      Use MIDI port N (0-based index)\n");
    printf("  --virtual NAME    Create virtual MIDI port with NAME\n");
    printf("\n");
    printf("Built-in Synth Options:\n");
    printf("  -sf, --soundfont PATH  Use built-in synth with soundfont (.sf2)\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s bog                   Start interactive Bog REPL\n", prog);
    printf("  %s bog song.bog          Execute a Bog file\n", prog);
    printf("  %s bog -sf gm.sf2        REPL with built-in synth\n", prog);
    printf("  %s bog --virtual BogOut  REPL with virtual MIDI port\n", prog);
    printf("\n");
}

static void print_bog_repl_help(void) {
    shared_print_command_help();

    printf("Bog-specific Commands:\n");
    printf("  :play FILE        Load and execute a Bog file\n");
    printf("  :tempo BPM        Set tempo (default: 120)\n");
    printf("  :swing AMOUNT     Set swing (0.0-1.0, default: 0.0)\n");
    printf("\n");
    printf("Slot Commands (named rules):\n");
    printf("  :def NAME RULE    Define/replace a named slot (:d for short)\n");
    printf("  :undef NAME       Remove a named slot (:u for short)\n");
    printf("  :slots            Show all slots (:ls for short)\n");
    printf("  :clear            Remove all slots\n");
    printf("  :mute NAME        Mute a slot (keeps rule, stops sound)\n");
    printf("  :unmute NAME      Unmute a slot\n");
    printf("  :solo NAME        Mute all except named slot\n");
    printf("  :unsolo           Unmute all slots\n");
    printf("\n");
    printf("Bog Syntax:\n");
    printf("  event(Voice, Pitch, Vel, T) :- Condition.\n");
    printf("\n");
    printf("Conditions:\n");
    printf("  every(T, N)       Fire every N beats (0.5 = 8th notes)\n");
    printf("  beat(T, N)        Fire on beat N of the bar\n");
    printf("  euc(T, K, N, B, R) Euclidean rhythm: K hits over N steps\n");
    printf("\n");
    printf("Available Voices:\n");
    printf("  kick, snare, hat, clap, noise   (drums, channel 10)\n");
    printf("  sine, square, triangle          (melodic, channel 1)\n");
    printf("\n");
    printf("Examples:\n");
    printf("  :d kick event(kick, 36, 0.9, T) :- every(T, 1.0).\n");
    printf("  :d hat  event(hat, 42, 0.5, T) :- every(T, 0.25).\n");
    printf("  :mute kick\n");
    printf("  :u hat\n");
    printf("\n");
}

/* ============================================================================
 * Bog REPL State
 * ============================================================================ */

/* MIDI note mappings for drum sounds (GM drums, channel 10) */
#define BOG_MIDI_KICK  36
#define BOG_MIDI_SNARE 38
#define BOG_MIDI_HAT   42
#define BOG_MIDI_CLAP  39
#define BOG_MIDI_NOISE 46

/* MIDI channels */
#define BOG_DRUM_CHANNEL  10
#define BOG_SYNTH_CHANNEL 1

/* Bog REPL state */
static BogArena *g_bog_repl_arena = NULL;
static BogBuiltins *g_bog_repl_builtins = NULL;
static BogStateManager *g_bog_repl_state_manager = NULL;
static BogScheduler *g_bog_repl_scheduler = NULL;
static BogTransitionManager *g_bog_repl_transition = NULL;
static BogLiveEvaluator *g_bog_repl_evaluator = NULL;
static SharedContext *g_bog_repl_shared = NULL;
static int g_bog_repl_running = 0;
static double g_bog_repl_tempo = 120.0;
static double g_bog_repl_swing = 0.0;

/* Forward declaration */
static void bog_start_async(void);

/* ============================================================================
 * Named Slots for Rule Management
 * ============================================================================ */

typedef struct {
    char *name;
    char *rule_text;
    int muted;
} BogReplSlot;

static BogReplSlot *g_bog_slots = NULL;
static size_t g_bog_slot_count = 0;
static size_t g_bog_slot_capacity = 0;

/* Find slot by name, returns index or -1 if not found */
static int bog_slot_find(const char *name) {
    for (size_t i = 0; i < g_bog_slot_count; i++) {
        if (strcmp(g_bog_slots[i].name, name) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/* Add or replace a slot */
static int bog_slot_def(const char *name, const char *rule_text) {
    int idx = bog_slot_find(name);
    if (idx >= 0) {
        /* Replace existing */
        free(g_bog_slots[idx].rule_text);
        g_bog_slots[idx].rule_text = strdup(rule_text);
        g_bog_slots[idx].muted = 0;
        return 1; /* replaced */
    }
    /* Add new */
    if (g_bog_slot_count >= g_bog_slot_capacity) {
        size_t new_cap = g_bog_slot_capacity ? g_bog_slot_capacity * 2 : 16;
        BogReplSlot *new_slots = realloc(g_bog_slots, new_cap * sizeof(BogReplSlot));
        if (!new_slots) return -1;
        g_bog_slots = new_slots;
        g_bog_slot_capacity = new_cap;
    }
    g_bog_slots[g_bog_slot_count].name = strdup(name);
    g_bog_slots[g_bog_slot_count].rule_text = strdup(rule_text);
    g_bog_slots[g_bog_slot_count].muted = 0;
    g_bog_slot_count++;
    return 0; /* added */
}

/* Remove a slot by name */
static int bog_slot_undef(const char *name) {
    int idx = bog_slot_find(name);
    if (idx < 0) return -1;
    free(g_bog_slots[idx].name);
    free(g_bog_slots[idx].rule_text);
    /* Shift remaining slots */
    for (size_t i = idx; i < g_bog_slot_count - 1; i++) {
        g_bog_slots[i] = g_bog_slots[i + 1];
    }
    g_bog_slot_count--;
    return 0;
}

/* Mute/unmute a slot */
static int bog_slot_mute(const char *name, int muted) {
    int idx = bog_slot_find(name);
    if (idx < 0) return -1;
    g_bog_slots[idx].muted = muted;
    return 0;
}

/* Clear all slots */
static void bog_slot_clear(void) {
    for (size_t i = 0; i < g_bog_slot_count; i++) {
        free(g_bog_slots[i].name);
        free(g_bog_slots[i].rule_text);
    }
    g_bog_slot_count = 0;
}

/* Free all slot memory */
static void bog_slot_cleanup(void) {
    bog_slot_clear();
    free(g_bog_slots);
    g_bog_slots = NULL;
    g_bog_slot_capacity = 0;
}

/* Build concatenated program from non-muted slots */
static char *bog_slot_build_program(void) {
    size_t total_len = 0;
    for (size_t i = 0; i < g_bog_slot_count; i++) {
        if (!g_bog_slots[i].muted) {
            total_len += strlen(g_bog_slots[i].rule_text) + 1; /* +1 for newline */
        }
    }
    if (total_len == 0) return strdup("");

    char *program = malloc(total_len + 1);
    if (!program) return NULL;

    char *p = program;
    for (size_t i = 0; i < g_bog_slot_count; i++) {
        if (!g_bog_slots[i].muted) {
            size_t len = strlen(g_bog_slots[i].rule_text);
            memcpy(p, g_bog_slots[i].rule_text, len);
            p += len;
            *p++ = '\n';
        }
    }
    *p = '\0';
    return program;
}

/* Evaluate the current slot program */
static int bog_slot_evaluate(void) {
    char *program = bog_slot_build_program();
    if (!program) return -1;

    char *error = NULL;
    int ok = 1;

    if (g_bog_repl_evaluator) {
        if (*program) {
            ok = bog_live_evaluator_evaluate(g_bog_repl_evaluator, program, &error);
        } else {
            /* Empty program - clear scheduler */
            ok = bog_live_evaluator_evaluate(g_bog_repl_evaluator, "% empty", &error);
        }
    }

    free(program);

    if (!ok) {
        printf("Error: %s\n", error ? error : "Parse error");
        if (error) free(error);
        return -1;
    }

    /* Start scheduler if not running */
    if (!g_bog_repl_running && g_bog_repl_scheduler && g_bog_slot_count > 0) {
        bog_scheduler_start(g_bog_repl_scheduler);
        g_bog_repl_running = 1;
        bog_start_async();
    }

    return 0;
}

/* ============================================================================
 * Audio Callbacks
 * ============================================================================ */

static int vel_to_midi(double velocity) {
    int v = (int)(velocity * 127.0);
    if (v < 0) v = 0;
    if (v > 127) v = 127;
    return v;
}

/* Track start time for audio clock */
static double g_bog_start_time = 0.0;

static double get_wall_time(void) {
#ifdef _WIN32
    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER counter;
    if (freq.QuadPart == 0) {
        QueryPerformanceFrequency(&freq);
    }
    QueryPerformanceCounter(&counter);
    return (double)counter.QuadPart / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1000000000.0;
#endif
}

static void repl_audio_init(void *userdata) {
    (void)userdata;
    /* Initialize start time for the audio clock */
    g_bog_start_time = get_wall_time();
}

static double repl_audio_time(void *userdata) {
    (void)userdata;
    return get_wall_time() - g_bog_start_time;
}

static void repl_audio_kick(void *userdata, double time, double velocity) {
    (void)userdata;
    (void)time;
    if (!g_bog_repl_shared) return;
    shared_send_note_on(g_bog_repl_shared, BOG_DRUM_CHANNEL, BOG_MIDI_KICK, vel_to_midi(velocity));
}

static void repl_audio_snare(void *userdata, double time, double velocity) {
    (void)userdata;
    (void)time;
    if (!g_bog_repl_shared) return;
    shared_send_note_on(g_bog_repl_shared, BOG_DRUM_CHANNEL, BOG_MIDI_SNARE, vel_to_midi(velocity));
}

static void repl_audio_hat(void *userdata, double time, double velocity) {
    (void)userdata;
    (void)time;
    if (!g_bog_repl_shared) return;
    shared_send_note_on(g_bog_repl_shared, BOG_DRUM_CHANNEL, BOG_MIDI_HAT, vel_to_midi(velocity));
}

static void repl_audio_clap(void *userdata, double time, double velocity) {
    (void)userdata;
    (void)time;
    if (!g_bog_repl_shared) return;
    shared_send_note_on(g_bog_repl_shared, BOG_DRUM_CHANNEL, BOG_MIDI_CLAP, vel_to_midi(velocity));
}

static void repl_audio_sine(void *userdata, double time, double midi, double velocity) {
    (void)userdata;
    (void)time;
    if (!g_bog_repl_shared) return;
    int pitch = (int)midi;
    if (pitch < 0) pitch = 0;
    if (pitch > 127) pitch = 127;
    shared_send_note_on(g_bog_repl_shared, BOG_SYNTH_CHANNEL, pitch, vel_to_midi(velocity));
}

static void repl_audio_square(void *userdata, double time, double midi, double velocity) {
    (void)userdata;
    (void)time;
    if (!g_bog_repl_shared) return;
    int pitch = (int)midi;
    if (pitch < 0) pitch = 0;
    if (pitch > 127) pitch = 127;
    shared_send_note_on(g_bog_repl_shared, BOG_SYNTH_CHANNEL + 1, pitch, vel_to_midi(velocity));
}

static void repl_audio_triangle(void *userdata, double time, double midi, double velocity) {
    (void)userdata;
    (void)time;
    if (!g_bog_repl_shared) return;
    int pitch = (int)midi;
    if (pitch < 0) pitch = 0;
    if (pitch > 127) pitch = 127;
    shared_send_note_on(g_bog_repl_shared, BOG_SYNTH_CHANNEL + 2, pitch, vel_to_midi(velocity));
}

static void repl_audio_noise(void *userdata, double time, double velocity) {
    (void)userdata;
    (void)time;
    if (!g_bog_repl_shared) return;
    shared_send_note_on(g_bog_repl_shared, BOG_DRUM_CHANNEL, BOG_MIDI_NOISE, vel_to_midi(velocity));
}

/* ============================================================================
 * Bog Tab Completion
 * ============================================================================ */

/* Bog completion words: built-in predicates + voice names */
static const char *bog_completion_words[] = {
    /* Voice names */
    "kick", "snare", "hat", "clap", "noise",
    "sine", "square", "triangle",
    /* Main predicate */
    "event",
    /* Timing predicates */
    "every", "beat", "phase", "euc",
    /* Comparison */
    "eq", "lt", "gt", "lte", "gte", "within",
    /* Arithmetic */
    "is", "add", "range",
    /* Music theory */
    "scale", "chord", "transpose", "rotate",
    /* Randomness */
    "prob", "rand", "randint", "choose", "pick", "cycle",
    /* State */
    "distinct", "cooldown",
};
#define BOG_COMPLETION_WORD_COUNT (sizeof(bog_completion_words) / sizeof(bog_completion_words[0]))

/* ============================================================================
 * Bog REPL Loop
 * ============================================================================ */

/* Stop callback for Bog REPL */
static void bog_stop_playback(void) {
    /* Stop async thread first */
    bog_async_stop();

    if (g_bog_repl_scheduler && g_bog_repl_running) {
        bog_scheduler_stop(g_bog_repl_scheduler);
        g_bog_repl_running = 0;
    }
    if (g_bog_repl_state_manager) {
        bog_state_manager_reset(g_bog_repl_state_manager);
    }
    if (g_bog_repl_shared) {
        shared_send_panic(g_bog_repl_shared);
    }
}

/* Note: Using repl_repl_starts_with() from loki/repl_helpers.h */

/* Process a Bog REPL command. Returns: 0=continue, 1=quit, 2=evaluate as Bog */
static int bog_process_command(const char *input) {
    /* Try shared commands first */
    int result = shared_process_command(g_bog_repl_shared, input, bog_stop_playback);
    if (result == REPL_CMD_QUIT) {
        return 1; /* quit */
    }
    if (result == REPL_CMD_HANDLED) {
        return 0;
    }

    /* Handle Bog-specific commands */
    const char *cmd = input;
    if (cmd[0] == ':')
        cmd++;

    /* Help */
    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "h") == 0 || strcmp(cmd, "?") == 0) {
        print_bog_repl_help();
        return 0;
    }

    /* :tempo BPM */
    if (repl_starts_with(cmd, "tempo ")) {
        double bpm = atof(cmd + 6);
        if (bpm >= 20.0 && bpm <= 400.0) {
            g_bog_repl_tempo = bpm;
            if (g_bog_repl_scheduler) {
                bog_scheduler_configure(g_bog_repl_scheduler, bpm, g_bog_repl_swing, 50.0, 0.25);
            }
            printf("Tempo: %.1f BPM\n", bpm);
        } else {
            printf("Invalid tempo (20-400)\n");
        }
        return 0;
    }

    /* :swing AMOUNT */
    if (repl_starts_with(cmd, "swing ")) {
        double swing = atof(cmd + 6);
        if (swing >= 0.0 && swing <= 1.0) {
            g_bog_repl_swing = swing;
            if (g_bog_repl_scheduler) {
                bog_scheduler_configure(g_bog_repl_scheduler, g_bog_repl_tempo, swing, 50.0, 0.25);
            }
            printf("Swing: %.2f\n", swing);
        } else {
            printf("Invalid swing (0.0-1.0)\n");
        }
        return 0;
    }

    /* :play FILE */
    if (repl_starts_with(cmd, "play ")) {
        const char *path = cmd + 5;
        while (*path == ' ') path++;
        if (*path) {
            FILE *f = fopen(path, "r");
            if (!f) {
                printf("Error: Cannot open file: %s\n", path);
            } else {
                /* Read file contents */
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                fseek(f, 0, SEEK_SET);

                char *code = malloc(size + 1);
                if (code) {
                    size_t read = fread(code, 1, size, f);
                    code[read] = '\0';

                    /* Evaluate */
                    char *error = NULL;
                    if (g_bog_repl_evaluator) {
                        if (bog_live_evaluator_evaluate(g_bog_repl_evaluator, code, &error)) {
                            printf("Loaded: %s\n", path);
                            if (!g_bog_repl_running && g_bog_repl_scheduler) {
                                bog_scheduler_start(g_bog_repl_scheduler);
                                g_bog_repl_running = 1;
                            }
                        } else {
                            printf("Error: %s\n", error ? error : "Parse error");
                            if (error) free(error);
                        }
                    }
                    free(code);
                }
                fclose(f);
            }
        } else {
            printf("Usage: :play PATH\n");
        }
        return 0;
    }

    /* :def NAME RULE - define or replace a named slot */
    if (repl_starts_with(cmd, "def ") || repl_starts_with(cmd, "d ")) {
        const char *rest = cmd + (cmd[1] == 'e' ? 4 : 2);
        while (*rest == ' ') rest++;
        /* Parse name (until space) */
        const char *name_start = rest;
        while (*rest && *rest != ' ') rest++;
        if (rest == name_start || !*rest) {
            printf("Usage: :def NAME RULE\n");
            return 0;
        }
        size_t name_len = rest - name_start;
        char *name = malloc(name_len + 1);
        memcpy(name, name_start, name_len);
        name[name_len] = '\0';

        while (*rest == ' ') rest++;
        if (!*rest) {
            printf("Usage: :def NAME RULE\n");
            free(name);
            return 0;
        }

        int result = bog_slot_def(name, rest);
        if (result < 0) {
            printf("Error: Failed to define slot\n");
        } else if (result == 1) {
            printf("ok [%s replaced]\n", name);
        } else {
            printf("ok [%s]\n", name);
        }
        free(name);

        bog_slot_evaluate();
        return 0;
    }

    /* :undef NAME - remove a named slot */
    if (repl_starts_with(cmd, "undef ") || repl_starts_with(cmd, "u ")) {
        const char *name = cmd + (cmd[1] == 'n' ? 6 : 2);
        while (*name == ' ') name++;
        if (!*name) {
            printf("Usage: :undef NAME\n");
            return 0;
        }
        /* Trim trailing space */
        char *name_copy = strdup(name);
        char *end = name_copy + strlen(name_copy) - 1;
        while (end > name_copy && *end == ' ') *end-- = '\0';

        if (bog_slot_undef(name_copy) == 0) {
            printf("ok [%s removed]\n", name_copy);
            bog_slot_evaluate();
        } else {
            printf("Error: No slot named '%s'\n", name_copy);
        }
        free(name_copy);
        return 0;
    }

    /* :slots or :ls - show all slots */
    if (strcmp(cmd, "slots") == 0 || strcmp(cmd, "ls") == 0) {
        if (g_bog_slot_count == 0) {
            printf("No slots defined\n");
        } else {
            printf("Slots (%zu):\n", g_bog_slot_count);
            for (size_t i = 0; i < g_bog_slot_count; i++) {
                printf("  %s: %s%s\n",
                       g_bog_slots[i].name,
                       g_bog_slots[i].rule_text,
                       g_bog_slots[i].muted ? " [muted]" : "");
            }
        }
        return 0;
    }

    /* :clear - remove all slots */
    if (strcmp(cmd, "clear") == 0) {
        bog_slot_clear();
        bog_slot_evaluate();
        printf("All slots cleared\n");
        return 0;
    }

    /* :mute NAME - mute a slot */
    if (repl_starts_with(cmd, "mute ")) {
        const char *name = cmd + 5;
        while (*name == ' ') name++;
        char *name_copy = strdup(name);
        char *end = name_copy + strlen(name_copy) - 1;
        while (end > name_copy && *end == ' ') *end-- = '\0';

        if (bog_slot_mute(name_copy, 1) == 0) {
            printf("ok [%s muted]\n", name_copy);
            bog_slot_evaluate();
        } else {
            printf("Error: No slot named '%s'\n", name_copy);
        }
        free(name_copy);
        return 0;
    }

    /* :unmute NAME - unmute a slot */
    if (repl_starts_with(cmd, "unmute ")) {
        const char *name = cmd + 7;
        while (*name == ' ') name++;
        char *name_copy = strdup(name);
        char *end = name_copy + strlen(name_copy) - 1;
        while (end > name_copy && *end == ' ') *end-- = '\0';

        if (bog_slot_mute(name_copy, 0) == 0) {
            printf("ok [%s unmuted]\n", name_copy);
            bog_slot_evaluate();
        } else {
            printf("Error: No slot named '%s'\n", name_copy);
        }
        free(name_copy);
        return 0;
    }

    /* :solo NAME - mute all except named slot */
    if (repl_starts_with(cmd, "solo ")) {
        const char *name = cmd + 5;
        while (*name == ' ') name++;
        char *name_copy = strdup(name);
        char *end = name_copy + strlen(name_copy) - 1;
        while (end > name_copy && *end == ' ') *end-- = '\0';

        int found = bog_slot_find(name_copy);
        if (found < 0) {
            printf("Error: No slot named '%s'\n", name_copy);
        } else {
            for (size_t i = 0; i < g_bog_slot_count; i++) {
                g_bog_slots[i].muted = (i != (size_t)found);
            }
            printf("ok [solo %s]\n", name_copy);
            bog_slot_evaluate();
        }
        free(name_copy);
        return 0;
    }

    /* :unsolo - unmute all slots */
    if (strcmp(cmd, "unsolo") == 0) {
        for (size_t i = 0; i < g_bog_slot_count; i++) {
            g_bog_slots[i].muted = 0;
        }
        printf("ok [all unmuted]\n");
        bog_slot_evaluate();
        return 0;
    }

    return 2; /* evaluate as Bog */
}

/* Start async tick thread */
static void bog_start_async(void) {
    if (g_bog_repl_scheduler && g_bog_repl_running) {
        bog_async_start(g_bog_repl_scheduler, g_bog_repl_transition);
    }
}

/* Non-interactive Bog REPL loop for piped input */
static void bog_repl_loop_pipe(void) {
    char line[4096];

    while (fgets(line, sizeof(line), stdin) != NULL) {
        /* Strip trailing newline */
        size_t len = repl_strip_newlines(line);
        if (len == 0) continue;

        int result = bog_process_command(line);
        if (result == 1) {
            break;      /* quit */
        }
        if (result == 0) continue;   /* command handled */

        /* Evaluate as Bog code */
        char *error = NULL;
        if (g_bog_repl_evaluator) {
            if (bog_live_evaluator_evaluate(g_bog_repl_evaluator, line, &error)) {
                printf("ok\n");
                if (!g_bog_repl_running && g_bog_repl_scheduler) {
                    bog_scheduler_start(g_bog_repl_scheduler);
                    g_bog_repl_running = 1;
                    bog_start_async();
                }
            } else {
                printf("Error: %s\n", error ? error : "Parse error");
                if (error) free(error);
            }
        }
        fflush(stdout);
    }
}

static void bog_repl_loop(editor_ctx_t *syntax_ctx) {
    ReplLineEditor ed;
    char *input;
    char history_path[512] = {0};

    /* Start async thread if scheduler is running */
    bog_start_async();

    /* Use non-interactive mode for piped input */
    if (!isatty(STDIN_FILENO)) {
        bog_repl_loop_pipe();
        bog_async_stop();
        return;
    }

    repl_editor_init(&ed);

    /* Set up tab completion for Bog predicates and voices */
    repl_set_completion_words(&ed, bog_completion_words, BOG_COMPLETION_WORD_COUNT);

    /* Build history file path and load history */
    if (repl_get_history_path("bog", history_path, sizeof(history_path))) {
        repl_history_load(&ed, history_path);
    }

    printf("Bog REPL %s (type :h for help, :q to quit)\n", PSND_VERSION);
    fflush(stdout);

    /* Enable raw mode for syntax-highlighted input */
    repl_enable_raw_mode();

    while (1) {
        /* Use standard blocking readline - async thread handles scheduler ticking */
        input = repl_readline(syntax_ctx, &ed, "bog> ");

        if (input == NULL) {
            /* EOF - exit cleanly */
            break;
        }

        if (input[0] == '\0') {
            continue;
        }

        repl_add_history(&ed, input);

        /* Process command */
        int result = bog_process_command(input);
        if (result == 1) break;      /* quit */
        if (result == 0) {
            shared_repl_link_check();
            continue;   /* command handled */
        }

        /* Evaluate Bog code */
        char *error = NULL;
        if (g_bog_repl_evaluator) {
            if (bog_live_evaluator_evaluate(g_bog_repl_evaluator, input, &error)) {
                printf("ok\n");
                if (!g_bog_repl_running && g_bog_repl_scheduler) {
                    bog_scheduler_start(g_bog_repl_scheduler);
                    g_bog_repl_running = 1;
                    bog_start_async();
                }
            } else {
                printf("Error: %s\n", error ? error : "Parse error");
                if (error) free(error);
            }
        }
        shared_repl_link_check();
    }

    /* Stop async thread before cleanup */
    bog_async_stop();

    /* Disable raw mode before exit */
    repl_disable_raw_mode();

    /* Save history */
    if (history_path[0]) {
        repl_history_save(&ed, history_path);
    }

    repl_editor_cleanup(&ed);
}

/* ============================================================================
 * Shared REPL Launcher Callbacks
 * ============================================================================ */

/* List MIDI ports */
static void bog_cb_list_ports(void) {
    SharedContext tmp;
    shared_context_init(&tmp);
    shared_midi_list_ports(&tmp);
    shared_context_cleanup(&tmp);
}

/* Initialize Bog context and MIDI/audio */
static void *bog_cb_init(const SharedReplArgs *args) {
    /* Initialize async system */
    bog_async_init();

    /* Create arena */
    g_bog_repl_arena = bog_arena_create();
    if (!g_bog_repl_arena) {
        fprintf(stderr, "Error: Failed to create Bog arena\n");
        return NULL;
    }

    /* Create builtins */
    g_bog_repl_builtins = bog_create_builtins(g_bog_repl_arena);
    if (!g_bog_repl_builtins) {
        fprintf(stderr, "Error: Failed to create Bog builtins\n");
        bog_arena_destroy(g_bog_repl_arena);
        g_bog_repl_arena = NULL;
        return NULL;
    }

    /* Create state manager */
    g_bog_repl_state_manager = bog_state_manager_create();
    if (!g_bog_repl_state_manager) {
        fprintf(stderr, "Error: Failed to create Bog state manager\n");
        bog_arena_destroy(g_bog_repl_arena);
        g_bog_repl_arena = NULL;
        return NULL;
    }

    /* Setup audio callbacks */
    BogAudioCallbacks audio = {
        .userdata = NULL,
        .init = repl_audio_init,
        .time = repl_audio_time,
        .kick = repl_audio_kick,
        .snare = repl_audio_snare,
        .hat = repl_audio_hat,
        .clap = repl_audio_clap,
        .sine = repl_audio_sine,
        .square = repl_audio_square,
        .triangle = repl_audio_triangle,
        .noise = repl_audio_noise,
    };

    /* Create scheduler */
    g_bog_repl_scheduler = bog_scheduler_create(&audio, g_bog_repl_builtins, g_bog_repl_state_manager);
    if (!g_bog_repl_scheduler) {
        fprintf(stderr, "Error: Failed to create Bog scheduler\n");
        bog_state_manager_destroy(g_bog_repl_state_manager);
        bog_arena_destroy(g_bog_repl_arena);
        g_bog_repl_state_manager = NULL;
        g_bog_repl_arena = NULL;
        return NULL;
    }

    /* Configure scheduler */
    bog_scheduler_configure(g_bog_repl_scheduler, g_bog_repl_tempo, g_bog_repl_swing, 50.0, 0.25);

    /* Create transition manager */
    g_bog_repl_transition = bog_transition_manager_create(g_bog_repl_scheduler, 4.0);
    if (!g_bog_repl_transition) {
        fprintf(stderr, "Error: Failed to create Bog transition manager\n");
        bog_scheduler_destroy(g_bog_repl_scheduler);
        bog_state_manager_destroy(g_bog_repl_state_manager);
        bog_arena_destroy(g_bog_repl_arena);
        g_bog_repl_scheduler = NULL;
        g_bog_repl_state_manager = NULL;
        g_bog_repl_arena = NULL;
        return NULL;
    }

    /* Create live evaluator */
    g_bog_repl_evaluator = bog_live_evaluator_create(g_bog_repl_scheduler, 0.1);
    if (!g_bog_repl_evaluator) {
        fprintf(stderr, "Error: Failed to create Bog live evaluator\n");
        bog_transition_manager_destroy(g_bog_repl_transition);
        bog_scheduler_destroy(g_bog_repl_scheduler);
        bog_state_manager_destroy(g_bog_repl_state_manager);
        bog_arena_destroy(g_bog_repl_arena);
        g_bog_repl_transition = NULL;
        g_bog_repl_scheduler = NULL;
        g_bog_repl_state_manager = NULL;
        g_bog_repl_arena = NULL;
        return NULL;
    }

    /* Initialize shared context for MIDI/audio */
    g_bog_repl_shared = calloc(1, sizeof(SharedContext));
    if (!g_bog_repl_shared) {
        fprintf(stderr, "Error: Failed to create shared context\n");
        bog_live_evaluator_destroy(g_bog_repl_evaluator);
        bog_transition_manager_destroy(g_bog_repl_transition);
        bog_scheduler_destroy(g_bog_repl_scheduler);
        bog_state_manager_destroy(g_bog_repl_state_manager);
        bog_arena_destroy(g_bog_repl_arena);
        g_bog_repl_evaluator = NULL;
        g_bog_repl_transition = NULL;
        g_bog_repl_scheduler = NULL;
        g_bog_repl_state_manager = NULL;
        g_bog_repl_arena = NULL;
        return NULL;
    }
    shared_context_init(g_bog_repl_shared);

    /* Setup output */
    if (args->soundfont_path) {
        /* Use built-in synth */
        if (shared_tsf_init() != 0 || shared_tsf_load_soundfont(args->soundfont_path) != 0) {
            fprintf(stderr, "Error: Failed to load soundfont: %s\n", args->soundfont_path);
            shared_context_cleanup(g_bog_repl_shared);
            free(g_bog_repl_shared);
            bog_live_evaluator_destroy(g_bog_repl_evaluator);
            bog_transition_manager_destroy(g_bog_repl_transition);
            bog_scheduler_destroy(g_bog_repl_scheduler);
            bog_state_manager_destroy(g_bog_repl_state_manager);
            bog_arena_destroy(g_bog_repl_arena);
            g_bog_repl_shared = NULL;
            g_bog_repl_evaluator = NULL;
            g_bog_repl_transition = NULL;
            g_bog_repl_scheduler = NULL;
            g_bog_repl_state_manager = NULL;
            g_bog_repl_arena = NULL;
            return NULL;
        }
        g_bog_repl_shared->builtin_synth_enabled = 1;
        if (args->verbose) {
            printf("Using built-in synth: %s\n", args->soundfont_path);
        }
    } else {
        /* Setup MIDI output */
        int midi_opened = 0;

        if (args->virtual_name) {
            if (shared_midi_open_virtual(g_bog_repl_shared, args->virtual_name) == 0) {
                midi_opened = 1;
                if (args->verbose) {
                    printf("Created virtual MIDI port: %s\n", args->virtual_name);
                }
            }
        } else if (args->port_index >= 0) {
            if (shared_midi_open_port(g_bog_repl_shared, args->port_index) == 0) {
                midi_opened = 1;
            }
        } else {
            /* Try to open a virtual port by default */
            if (shared_midi_open_virtual(g_bog_repl_shared, PSND_MIDI_PORT_NAME) == 0) {
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

    /* Initialize Link callbacks for REPL notifications */
    shared_repl_link_init_callbacks(g_bog_repl_shared);

    /* Return non-NULL to indicate success */
    return g_bog_repl_scheduler;
}

/* Cleanup Bog context and MIDI/audio */
static void bog_cb_cleanup(void *lang_ctx) {
    (void)lang_ctx;

    /* Cleanup Link callbacks */
    shared_repl_link_cleanup_callbacks();

    /* Stop async thread first */
    bog_async_stop();
    bog_async_cleanup();

    /* Stop scheduler */
    if (g_bog_repl_scheduler && g_bog_repl_running) {
        bog_scheduler_stop(g_bog_repl_scheduler);
        g_bog_repl_running = 0;
    }

    /* Wait for audio buffer to drain */
    if (g_bog_repl_shared && g_bog_repl_shared->builtin_synth_enabled) {
        usleep(300000);  /* 300ms for audio tail */
    }

    /* Cleanup */
    if (g_bog_repl_shared) {
        shared_send_panic(g_bog_repl_shared);
        shared_context_cleanup(g_bog_repl_shared);
        free(g_bog_repl_shared);
        g_bog_repl_shared = NULL;
    }

    if (g_bog_repl_evaluator) {
        bog_live_evaluator_destroy(g_bog_repl_evaluator);
        g_bog_repl_evaluator = NULL;
    }
    if (g_bog_repl_transition) {
        bog_transition_manager_destroy(g_bog_repl_transition);
        g_bog_repl_transition = NULL;
    }
    if (g_bog_repl_scheduler) {
        bog_scheduler_destroy(g_bog_repl_scheduler);
        g_bog_repl_scheduler = NULL;
    }
    if (g_bog_repl_state_manager) {
        bog_state_manager_destroy(g_bog_repl_state_manager);
        g_bog_repl_state_manager = NULL;
    }
    /* Cleanup slots */
    bog_slot_cleanup();

    if (g_bog_repl_arena) {
        bog_arena_destroy(g_bog_repl_arena);
        g_bog_repl_arena = NULL;
    }
}

/* Load and evaluate a Bog file (non-blocking - just loads, doesn't run loop) */
static int bog_load_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "Error: Cannot open file: %s\n", path);
        return -1;
    }

    /* Read file contents */
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *code = malloc(size + 1);
    if (!code) {
        fclose(f);
        fprintf(stderr, "Error: Out of memory\n");
        return -1;
    }

    size_t read_bytes = fread(code, 1, size, f);
    code[read_bytes] = '\0';
    fclose(f);

    /* Evaluate */
    char *error = NULL;
    int success = 0;
    if (g_bog_repl_evaluator) {
        success = bog_live_evaluator_evaluate(g_bog_repl_evaluator, code, &error);
    }
    free(code);

    if (!success) {
        fprintf(stderr, "Error: %s\n", error ? error : "Parse error");
        if (error) free(error);
        return -1;
    }

    /* Start scheduler if not already running */
    if (g_bog_repl_scheduler && !g_bog_repl_running) {
        bog_scheduler_start(g_bog_repl_scheduler);
        g_bog_repl_running = 1;
        /* NOTE: async thread will be started when entering REPL loop */
    }

    return 0;
}

/* Execute a Bog file - for headless 'play' mode with signal handler */
static volatile int g_bog_interrupted = 0;

#ifdef _WIN32
static BOOL WINAPI bog_console_handler(DWORD ctrl_type) {
    if (ctrl_type == CTRL_C_EVENT || ctrl_type == CTRL_BREAK_EVENT) {
        g_bog_interrupted = 1;
        g_bog_repl_running = 0;
        return TRUE;
    }
    return FALSE;
}
#else
static void bog_sigint_handler(int sig) {
    (void)sig;
    g_bog_interrupted = 1;
    g_bog_repl_running = 0;
}
#endif

static int bog_cb_exec_file(void *lang_ctx, const char *path, int verbose) {
    (void)lang_ctx;
    (void)verbose;

    if (bog_load_file(path) != 0) {
        return 1;
    }

    /* Install signal handler for Ctrl-C */
#ifdef _WIN32
    SetConsoleCtrlHandler(bog_console_handler, TRUE);
#else
    struct sigaction sa, old_sa;
    sa.sa_handler = bog_sigint_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT, &sa, &old_sa);
#endif

    /* Reset interrupt flag */
    g_bog_interrupted = 0;

    printf("Playing %s (press Ctrl-C to stop)...\n", path);
    fflush(stdout);

    /* Run scheduler loop until interrupted */
    while (g_bog_repl_running && !g_bog_interrupted) {
        bog_scheduler_tick(g_bog_repl_scheduler);
        if (g_bog_repl_transition) {
            double now = bog_scheduler_now(g_bog_repl_scheduler);
            bog_transition_manager_process(g_bog_repl_transition, now);
        }
        usleep(10000);  /* 10ms tick */
    }

    /* Stop scheduler */
    if (g_bog_repl_scheduler) {
        bog_scheduler_stop(g_bog_repl_scheduler);
        g_bog_repl_running = 0;
    }

    /* Restore original signal handler */
#ifdef _WIN32
    SetConsoleCtrlHandler(bog_console_handler, FALSE);
#else
    sigaction(SIGINT, &old_sa, NULL);
#endif

    if (g_bog_interrupted) {
        printf("\nStopped.\n");
    }

    return 0;
}

/* Run the Bog REPL loop */
static void bog_cb_repl_loop(void *lang_ctx, editor_ctx_t *syntax_ctx) {
    (void)lang_ctx;
    bog_repl_loop(syntax_ctx);
}

/* Bog shared REPL callbacks */
static const SharedReplCallbacks bog_repl_callbacks = {
    .name = "bog",
    .file_ext = ".bog",
    .prog_name = PSND_NAME,
    .print_usage = print_bog_repl_usage,
    .list_ports = bog_cb_list_ports,
    .init = bog_cb_init,
    .cleanup = bog_cb_cleanup,
    .exec_file = bog_cb_exec_file,
    .repl_loop = bog_cb_repl_loop,
};

/* ============================================================================
 * Bog REPL Main Entry Points
 * ============================================================================ */

/**
 * Custom bog_repl_main that supports file+REPL mode:
 * - `psnd bog` -> start REPL
 * - `psnd bog file.bog` -> load file, then start REPL (scheduler runs in background)
 */
int bog_repl_main(int argc, char **argv) {
    const char *input_file = NULL;
    SharedReplArgs args = {0, -1, NULL, NULL};
    int i;

    /* Parse arguments - skip argv[0] which is the language name ("bog") */
    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            print_bog_repl_usage(PSND_NAME);
            return 0;
        }
        if (strcmp(arg, "-l") == 0 || strcmp(arg, "--list") == 0) {
            bog_cb_list_ports();
            return 0;
        }
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0) {
            args.verbose = 1;
            continue;
        }
        if ((strcmp(arg, "-p") == 0 || strcmp(arg, "--port") == 0) && i + 1 < argc) {
            args.port_index = atoi(argv[++i]);
            continue;
        }
        if (strcmp(arg, "--virtual") == 0 && i + 1 < argc) {
            args.virtual_name = argv[++i];
            continue;
        }
        if ((strcmp(arg, "-sf") == 0 || strcmp(arg, "--soundfont") == 0) && i + 1 < argc) {
            args.soundfont_path = argv[++i];
            continue;
        }
        /* Non-option argument is input file */
        if (arg[0] != '-' && !input_file) {
            input_file = arg;
        }
    }
    args.soundfont_path = psnd_soundfont_resolve(args.soundfont_path,
        args.port_index >= 0 || args.virtual_name);

    /* Initialize bog */
    void *lang_ctx = bog_cb_init(&args);
    if (!lang_ctx) {
        fprintf(stderr, "Error: Failed to initialize Bog\n");
        return 1;
    }

    /* If file provided, load it (starts scheduler) */
    if (input_file) {
        if (args.verbose) {
            printf("Loading: %s\n", input_file);
        }
        if (bog_load_file(input_file) != 0) {
            bog_cb_cleanup(lang_ctx);
            return 1;
        }
        printf("Loaded: %s\n", input_file);
    }

    /* Setup syntax highlighting context */
    editor_ctx_t syntax_ctx;
    editor_ctx_init(&syntax_ctx);
    syntax_init_default_colors(&syntax_ctx);
    syntax_select_for_filename(&syntax_ctx, "input.bog");

    /* Load Lua for syntax highlighting */
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

    /* Enter REPL loop */
    bog_repl_loop(&syntax_ctx);

    /* Cleanup */
    if (syntax_ctx.lua_host) {
        lua_host_free(syntax_ctx.lua_host);
        syntax_ctx.lua_host = NULL;
    }
    editor_ctx_free(&syntax_ctx);
    bog_cb_cleanup(lang_ctx);

    return 0;
}

/**
 * Headless play mode - runs until Ctrl-C
 */
int bog_play_main(int argc, char **argv) {
    return shared_lang_play_main(&bog_repl_callbacks, argc, argv);
}
